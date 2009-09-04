/***************************************************************************
 *   Copyright (C) 2008-2009 by Andrzej Rybczak                            *
 *   electricityispower@gmail.com                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include "charset.h"
#include "browser.h"
#include "display.h"
#include "global.h"
#include "misc.h"
#include "mpdpp.h"
#include "playlist.h"
#include "playlist_editor.h"

using Global::MainHeight;
using Global::MainStartY;
using Global::myScreen;
using Global::myOldScreen;

SelectedItemsAdder *mySelectedItemsAdder = new SelectedItemsAdder;

void SelectedItemsAdder::Init()
{
	SetDimensions();
	w = new Menu<std::string>((COLS-itsWidth)/2, (MainHeight-itsHeight)/2+MainStartY, itsWidth, itsHeight, "Add selected items to...", Config.main_color, Config.window_border);
	w->SetTimeout(ncmpcpp_window_timeout);
	w->CyclicScrolling(Config.use_cyclic_scrolling);
	w->HighlightColor(Config.main_highlight_color);
	w->SetItemDisplayer(Display::Generic);
	isInitialized = 1;
}

void SelectedItemsAdder::SwitchTo()
{
	if (myScreen == this)
	{
		myOldScreen->SwitchTo();
		return;
	}
	if (!myScreen->allowsSelection())
		return;
	if (!myScreen->GetList()->hasSelected())
	{
		ShowMessage("No selected items!");
		return;
	}
	if (MainHeight < 5)
	{
		ShowMessage("Screen is too small to display this window!");
		return;
	}
	
	if (!isInitialized)
		Init();
	
	// Resize() can fall back to old screen, so we need it updated
	myOldScreen = myScreen;
	
	if (hasToBeResized)
		Resize();
	
	bool playlists_not_active = myScreen == myBrowser && Config.local_browser;
	if (playlists_not_active)
		ShowMessage("Local items cannot be added to m3u playlist!");
	
	w->Clear();
	w->Reset();
	w->AddOption("Current MPD playlist", 0, myOldScreen == myPlaylist);
	w->AddOption("Create new playlist", 0, playlists_not_active);
	w->AddSeparator();
	
	MPD::TagList playlists;
	Mpd.GetPlaylists(playlists);
	for (MPD::TagList::iterator it = playlists.begin(); it != playlists.end(); ++it)
	{
		utf_to_locale(*it);
		w->AddOption(*it, 0, playlists_not_active);
	}
	w->AddSeparator();
	w->AddOption("Cancel");
	
	myScreen = this;
	w->Window::Clear();
}

void SelectedItemsAdder::Resize()
{
	SetDimensions();
	if (itsHeight < 5) // screen too low to display this window
		return myOldScreen->SwitchTo();
	w->Resize(itsWidth, itsHeight);
	w->MoveTo((COLS-itsWidth)/2, (MainHeight-itsHeight)/2+MainStartY);
	if (myOldScreen && myOldScreen->hasToBeResized) // resize background window
	{
		myOldScreen->Resize();
		myOldScreen->Refresh();
	}
	hasToBeResized = 0;
}

std::basic_string<my_char_t> SelectedItemsAdder::Title()
{
	return myOldScreen->Title();
}

void SelectedItemsAdder::EnterPressed()
{
	size_t pos = w->Choice();
	
	MPD::SongList list;
	if (pos != w->Size()-1)
		myOldScreen->GetSelectedSongs(list);
	
	if (pos == 0) // add to mpd playlist
	{
		if (myPlaylist->Add(list, 0))
			ShowMessage("Selected items added!");
	}
	else if (pos == 1) // create new playlist
	{
		LockStatusbar();
		Statusbar() << "Save playlist as: ";
		std::string playlist = Global::wFooter->GetString();
		UnlockStatusbar();
		if (!playlist.empty())
		{
			std::string utf_playlist = locale_to_utf_cpy(playlist);
			Mpd.StartCommandsList();
			for (MPD::SongList::const_iterator it = list.begin(); it != list.end(); ++it)
				Mpd.AddToPlaylist(utf_playlist, **it);
			if (Mpd.CommitCommandsList())
				ShowMessage("Selected items added to playlist \"%s\"!", playlist.c_str());
		}
	}
	else if (pos > 1 && pos < w->Size()-1) // add items to existing playlist
	{
		std::string playlist = locale_to_utf_cpy(w->Current());
		Mpd.StartCommandsList();
		for (MPD::SongList::const_iterator it = list.begin(); it != list.end(); ++it)
			Mpd.AddToPlaylist(playlist, **it);
		if (Mpd.CommitCommandsList())
			ShowMessage("Selected items added to playlist \"%s\"!", w->Current().c_str());
	}
	if (pos != w->Size()-1)
	{
		// refresh playlist's lists
		if (!Config.local_browser && myBrowser->Main() && myBrowser->CurrentDir() == "/")
			myBrowser->GetDirectory("/");
		if (myPlaylistEditor->Main())
			myPlaylistEditor->Playlists->Clear(0); // make playlist editor update itself
	}
	MPD::FreeSongList(list);
	SwitchTo();
}

void SelectedItemsAdder::MouseButtonPressed(MEVENT me)
{
	if (w->Empty() || !w->hasCoords(me.x, me.y) || size_t(me.y) >= w->Size())
		return;
	if (me.bstate & (BUTTON1_PRESSED | BUTTON3_PRESSED))
	{
		w->Goto(me.y);
		if (me.bstate & BUTTON3_PRESSED)
			EnterPressed();
	}
	else
		Screen< Menu<std::string> >::MouseButtonPressed(me);
}

void SelectedItemsAdder::SetDimensions()
{
	itsWidth = COLS*0.6;
	itsHeight = std::min(size_t(LINES*0.6), MainHeight);
}


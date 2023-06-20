// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_CONSTANTS_H_

#import <string>

namespace overflow_menu {
enum class Destination {
  Bookmarks = 0,
  History = 1,
  ReadingList = 2,
  Passwords = 3,
  Downloads = 4,
  RecentTabs = 5,
  SiteInfo = 6,
  Settings = 7,
  WhatsNew = 8,
  SpotlightDebugger = 9,
  PriceNotifications = 10,
};

enum class ActionType {
  Follow = 0,
  Bookmarks,
  ReadingList,
  ClearBrowsingData,
  Translate,
  DesktopSite,
  FindInPage,
  TextZoom,
};

// Ingests `destination` string representation and returns corresponding
// overflow_menu::Destination enum.
Destination DestinationForStringName(std::string destination);

// Ingests overflow_menu::Destination `destination` and returns its string
// representation.
std::string StringNameForDestination(Destination destination);

// Ingests overflow_menu::Destination `destination` and records the
// corresponding UMA action.
void RecordUmaActionForDestination(Destination destination);
}  // namespace overflow_menu

using DestinationRanking = std::vector<overflow_menu::Destination>;
using ActionRanking = std::vector<overflow_menu::ActionType>;

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_CONSTANTS_H_

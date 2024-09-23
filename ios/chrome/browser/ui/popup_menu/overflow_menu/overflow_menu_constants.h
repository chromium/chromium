// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_CONSTANTS_H_

#import <string>
#import <vector>

namespace overflow_menu {
// LINT.IfChange(destination)
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
// LINT.ThenChange(overflow_menu_metrics.h:destination)

// Represents a type of action (i.e. a row). For example, both the Stop and
// Reload actions have an `actionType` of `Reload` as they would both take
// that position in the UI.
// LINT.IfChange(actionType)
enum class ActionType {
  Reload = 0,
  NewTab,
  NewIncognitoTab,
  NewWindow,
  Follow,
  Bookmark,
  ReadingList,
  ClearBrowsingData,
  Translate,
  DesktopSite,
  FindInPage,
  TextZoom,
  ReportAnIssue,
  Help,
  ShareChrome,
  EditActions,
  LensOverlay,
};
// LINT.ThenChange(overflow_menu_metrics.h:actionType)

// Ingests `destination` string representation and returns corresponding
// overflow_menu::Destination enum.
Destination DestinationForStringName(std::string destination);

// Ingests overflow_menu::Destination `destination` and returns its string
// representation.
std::string StringNameForDestination(Destination destination);

// Ingests `action` string representation and returns corresponding
// overflow_menu::ActionType enum.
ActionType ActionTypeForStringName(std::string action);

// Ingests overflow_menu::ActionType `action` and returns its string
// representation.
std::string StringNameForActionType(ActionType action);

// Ingests overflow_menu::Destination `destination` and records the
// corresponding UMA action.
void RecordUmaActionForDestination(Destination destination);
}  // namespace overflow_menu

using DestinationRanking = std::vector<overflow_menu::Destination>;
using ActionRanking = std::vector<overflow_menu::ActionType>;

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_CONSTANTS_H_

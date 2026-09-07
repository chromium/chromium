// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POPUP_MENU_OVERFLOW_MENU_PUBLIC_OVERFLOW_MENU_CONSTANTS_H_
#define IOS_CHROME_BROWSER_POPUP_MENU_OVERFLOW_MENU_PUBLIC_OVERFLOW_MENU_CONSTANTS_H_

#import <optional>
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
  Cobalt = 11,
  LevelUp = 12,
  DefaultBrowser = 13,
};
// LINT.ThenChange(
// /ios/chrome/browser/popup_menu/overflow_menu/ui/overflow_menu_metrics.h:destination,
// /tools/metrics/histograms/metadata/ios/enums.xml:IOSOverflowMenuDestination
// )

// Represents a type of action (i.e. a row). For example, both the Stop and
// Reload actions have an `actionType` of `Reload` as they would both take
// that position in the UI.
// LINT.IfChange(actionType)
enum class ActionType {
  Reload = 0,
  NewTab = 1,
  NewIncognitoTab = 2,
  NewWindow = 3,
  // Follow = 4, Deprecated in M143.
  Bookmark = 5,
  ReadingList = 6,
  ClearBrowsingData = 7,
  Translate = 8,
  DesktopSite = 9,
  FindInPage = 10,
  TextZoom = 11,
  ReportAnIssue = 12,
  Help = 13,
  ShareChrome = 14,
  EditActions = 15,
  LensOverlay = 16,
  AIPrototype = 17,
  SetTabReminder = 18,
  ReaderMode = 19,
  // TODO(crbug.com/416002705): Rename reference to BWG.
  AskBWG = 20,
  // HideToolbars = 21, Deprecated in M154.
  // TabGroup = 22, Deprecated in M150.
  ShareThisPage = 23,
  // Signin = 24, Deprecated in M152.
  Identity = 25,
  CustomizeHomePage = 26,
  DefaultBrowser = 27,
};
// LINT.ThenChange(/ios/chrome/browser/popup_menu/overflow_menu/ui/overflow_menu_metrics.h:actionType)

// Ingests `destination` string representation and returns corresponding
// overflow_menu::Destination enum.
std::optional<Destination> DestinationForStringName(std::string destination);

// Ingests overflow_menu::Destination `destination` and returns its string
// representation.
std::string StringNameForDestination(Destination destination);

// Ingests `action` string representation and returns corresponding
// overflow_menu::ActionType enum.
std::optional<ActionType> ActionTypeForStringName(std::string action);

// Ingests overflow_menu::ActionType `action` and returns its string
// representation.
std::string StringNameForActionType(ActionType action);

// Ingests overflow_menu::Destination `destination` and records the
// corresponding UMA action based on whether it is the NTP or not.
void RecordUmaActionForDestination(Destination destination, bool on_ntp);

// `kNewDestinationsInsertionIndex` represents the index new destinations are
// inserted into the current ranking.
constexpr int kNewDestinationsInsertionIndex = 3;
}  // namespace overflow_menu

using DestinationRanking = std::vector<overflow_menu::Destination>;
using ActionRanking = std::vector<overflow_menu::ActionType>;

#endif  // IOS_CHROME_BROWSER_POPUP_MENU_OVERFLOW_MENU_PUBLIC_OVERFLOW_MENU_CONSTANTS_H_

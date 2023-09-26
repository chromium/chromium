// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

IOSOverflowMenuDestination HistogramDestinationFromDestination(
    overflow_menu::Destination destination) {
  switch (destination) {
    case overflow_menu::Destination::Bookmarks:
      return IOSOverflowMenuDestination::kBookmarks;
    case overflow_menu::Destination::History:
      return IOSOverflowMenuDestination::kHistory;
    case overflow_menu::Destination::ReadingList:
      return IOSOverflowMenuDestination::kReadingList;
    case overflow_menu::Destination::Passwords:
      return IOSOverflowMenuDestination::kPasswords;
    case overflow_menu::Destination::Downloads:
      return IOSOverflowMenuDestination::kDownloads;
    case overflow_menu::Destination::RecentTabs:
      return IOSOverflowMenuDestination::kRecentTabs;
    case overflow_menu::Destination::SiteInfo:
      return IOSOverflowMenuDestination::kSiteInfo;
    case overflow_menu::Destination::Settings:
      return IOSOverflowMenuDestination::kSettings;
    case overflow_menu::Destination::WhatsNew:
      return IOSOverflowMenuDestination::kWhatsNew;
    case overflow_menu::Destination::SpotlightDebugger:
      return IOSOverflowMenuDestination::kSpotlightDebugger;
    case overflow_menu::Destination::PriceNotifications:
      return IOSOverflowMenuDestination::kPriceNotifications;
  }
}

IOSOverflowMenuAction HistogramActionFromActionType(
    overflow_menu::ActionType action_type) {
  switch (action_type) {
    case overflow_menu::ActionType::Reload:
      return IOSOverflowMenuAction::kReload;
    case overflow_menu::ActionType::NewTab:
      return IOSOverflowMenuAction::kNewTab;
    case overflow_menu::ActionType::NewIncognitoTab:
      return IOSOverflowMenuAction::kNewIncognitoTab;
    case overflow_menu::ActionType::NewWindow:
      return IOSOverflowMenuAction::kNewWindow;
    case overflow_menu::ActionType::Follow:
      return IOSOverflowMenuAction::kFollow;
    case overflow_menu::ActionType::Bookmark:
      return IOSOverflowMenuAction::kBookmark;
    case overflow_menu::ActionType::ReadingList:
      return IOSOverflowMenuAction::kReadingList;
    case overflow_menu::ActionType::ClearBrowsingData:
      return IOSOverflowMenuAction::kClearBrowsingData;
    case overflow_menu::ActionType::Translate:
      return IOSOverflowMenuAction::kTranslate;
    case overflow_menu::ActionType::DesktopSite:
      return IOSOverflowMenuAction::kDesktopSite;
    case overflow_menu::ActionType::FindInPage:
      return IOSOverflowMenuAction::kFindInPage;
    case overflow_menu::ActionType::TextZoom:
      return IOSOverflowMenuAction::kTextZoom;
    case overflow_menu::ActionType::ReportAnIssue:
      return IOSOverflowMenuAction::kReportAnIssue;
    case overflow_menu::ActionType::Help:
      return IOSOverflowMenuAction::kHelp;
    case overflow_menu::ActionType::ShareChrome:
      return IOSOverflowMenuAction::kShareChrome;
    case overflow_menu::ActionType::EditActions:
      return IOSOverflowMenuAction::kEditActions;
  }
}

void RecordDestinationsCustomizationEvent(
    DestinationsCustomizationEvent event) {
  base::UmaHistogramSparse(
      "IOS.OverflowMenu.Customization.DestinationsCustomized",
      event.ToEnumBitmask());
}

void RecordActionsCustomizationEvent(ActionsCustomizationEvent event) {
  base::UmaHistogramSparse("IOS.OverflowMenu.Customization.ActionsCustomized",
                           event.ToEnumBitmask());
}

void RecordOverflowMenuVisitedEvent(OverflowMenuVisitedEvent event) {
  base::UmaHistogramSparse("IOS.OverflowMenu.OverflowMenuVisited",
                           event.ToEnumBitmask());
}

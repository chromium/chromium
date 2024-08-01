// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

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
    case overflow_menu::ActionType::LensOverlay:
      return IOSOverflowMenuAction::kLensOverlay;
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

bool DestinationWasInitiallyVisible(
    overflow_menu::Destination destination,
    NSArray<OverflowMenuDestination*>* destinations,
    int visibleDestinationCount) {
  NSMutableArray<NSNumber*>* destinationInts = [[NSMutableArray alloc] init];
  for (OverflowMenuDestination* menuDestination in destinations) {
    [destinationInts
        addObject:[NSNumber numberWithInt:menuDestination.destination]];
  }
  return ItemWasInitiallyVisible(static_cast<int>(destination), destinationInts,
                                 visibleDestinationCount);
}

bool ActionWasInitiallyVisible(overflow_menu::ActionType actionType,
                               NSArray<OverflowMenuAction*>* actions,
                               int visibleActionCount) {
  NSMutableArray<NSNumber*>* actionTypes = [[NSMutableArray alloc] init];
  for (OverflowMenuAction* action in actions) {
    [actionTypes addObject:[NSNumber numberWithInt:action.actionType]];
  }
  return ItemWasInitiallyVisible(static_cast<int>(actionType), actionTypes,
                                 visibleActionCount);
}

bool ItemWasInitiallyVisible(int item,
                             NSArray<NSNumber*>* items,
                             int visibleItemCount) {
  // Check if the clicked destination was visible by default in the menu.
  int numItems = static_cast<int>(items.count);
  int rangeEnd = MAX(0, MIN(visibleItemCount, numItems));
  NSRange visibleItemRange = NSMakeRange(0, rangeEnd);
  NSArray<NSNumber*>* visibleItems = [items subarrayWithRange:visibleItemRange];
  for (NSNumber* arrayItem in visibleItems) {
    if (item == [arrayItem integerValue]) {
      // Item was visible by default, so don't log the event.
      return true;
    }
  }
  return false;
}

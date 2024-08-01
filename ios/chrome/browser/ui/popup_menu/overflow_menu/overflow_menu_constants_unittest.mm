// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class OverflowMenuConstantsTest : public PlatformTest {};

// Tests that all destinations can be converted to a string and back.
TEST_F(OverflowMenuConstantsTest, DestinationConversion) {
  // Loop through all enum int values until one is not caught in the switch
  // statement, signaling that all were handled.
  for (int value = 0;; value++) {
    overflow_menu::Destination destination =
        static_cast<overflow_menu::Destination>(value);
    std::optional<overflow_menu::Destination> finalExpectedDestination;
    switch (destination) {
      case overflow_menu::Destination::Bookmarks:
        finalExpectedDestination = overflow_menu::Destination::Bookmarks;
        break;
      case overflow_menu::Destination::History:
        finalExpectedDestination = overflow_menu::Destination::History;
        break;
      case overflow_menu::Destination::ReadingList:
        finalExpectedDestination = overflow_menu::Destination::ReadingList;
        break;
      case overflow_menu::Destination::Passwords:
        finalExpectedDestination = overflow_menu::Destination::Passwords;
        break;
      case overflow_menu::Destination::PriceNotifications:
        finalExpectedDestination =
            overflow_menu::Destination::PriceNotifications;
        break;
      case overflow_menu::Destination::Downloads:
        finalExpectedDestination = overflow_menu::Destination::Downloads;
        break;
      case overflow_menu::Destination::RecentTabs:
        finalExpectedDestination = overflow_menu::Destination::RecentTabs;
        break;
      case overflow_menu::Destination::SiteInfo:
        finalExpectedDestination = overflow_menu::Destination::SiteInfo;
        break;
      case overflow_menu::Destination::Settings:
        finalExpectedDestination = overflow_menu::Destination::Settings;
        break;
      case overflow_menu::Destination::WhatsNew:
        finalExpectedDestination = overflow_menu::Destination::WhatsNew;
        break;
      case overflow_menu::Destination::SpotlightDebugger:
        finalExpectedDestination =
            overflow_menu::Destination::SpotlightDebugger;
        break;
    }

    // If there's no finalExpectedDestination, then the loop has looped through
    // all possible enum values.
    if (!finalExpectedDestination) {
      break;
    }

    // This will fail if the destination was skipped in
    // DestinationForStringName.
    EXPECT_EQ(finalExpectedDestination,
              overflow_menu::DestinationForStringName(
                  overflow_menu::StringNameForDestination(destination)));
  }
}

// Tests that all action types can be converted to a string and back.
TEST_F(OverflowMenuConstantsTest, ActionTypeConversion) {
  // Loop through all enum int values until one is not caught in the switch
  // statement, signaling that all were handled.
  for (int value = 0;; value++) {
    overflow_menu::ActionType actionType =
        static_cast<overflow_menu::ActionType>(value);
    std::optional<overflow_menu::ActionType> finalExpectedActionType;
    switch (actionType) {
      case overflow_menu::ActionType::Reload:
        finalExpectedActionType = overflow_menu::ActionType::Reload;
        break;
      case overflow_menu::ActionType::NewTab:
        finalExpectedActionType = overflow_menu::ActionType::NewTab;
        break;
      case overflow_menu::ActionType::NewIncognitoTab:
        finalExpectedActionType = overflow_menu::ActionType::NewIncognitoTab;
        break;
      case overflow_menu::ActionType::NewWindow:
        finalExpectedActionType = overflow_menu::ActionType::NewWindow;
        break;
      case overflow_menu::ActionType::Follow:
        finalExpectedActionType = overflow_menu::ActionType::Follow;
        break;
      case overflow_menu::ActionType::Bookmark:
        finalExpectedActionType = overflow_menu::ActionType::Bookmark;
        break;
      case overflow_menu::ActionType::ReadingList:
        finalExpectedActionType = overflow_menu::ActionType::ReadingList;
        break;
      case overflow_menu::ActionType::ClearBrowsingData:
        finalExpectedActionType = overflow_menu::ActionType::ClearBrowsingData;
        break;
      case overflow_menu::ActionType::Translate:
        finalExpectedActionType = overflow_menu::ActionType::Translate;
        break;
      case overflow_menu::ActionType::DesktopSite:
        finalExpectedActionType = overflow_menu::ActionType::DesktopSite;
        break;
      case overflow_menu::ActionType::FindInPage:
        finalExpectedActionType = overflow_menu::ActionType::FindInPage;
        break;
      case overflow_menu::ActionType::TextZoom:
        finalExpectedActionType = overflow_menu::ActionType::TextZoom;
        break;
      case overflow_menu::ActionType::ReportAnIssue:
        finalExpectedActionType = overflow_menu::ActionType::ReportAnIssue;
        break;
      case overflow_menu::ActionType::Help:
        finalExpectedActionType = overflow_menu::ActionType::Help;
        break;
      case overflow_menu::ActionType::ShareChrome:
        finalExpectedActionType = overflow_menu::ActionType::ShareChrome;
        break;
      case overflow_menu::ActionType::EditActions:
        finalExpectedActionType = overflow_menu::ActionType::EditActions;
        break;
      case overflow_menu::ActionType::LensOverlay:
        finalExpectedActionType = overflow_menu::ActionType::LensOverlay;
        break;
    }

    // If there's no finalExpectedActionType, then the loop has looped through
    // all possible enum values.
    if (!finalExpectedActionType) {
      break;
    }

    // This will fail if the action type was skipped in ActionTypeForStringName.
    EXPECT_EQ(finalExpectedActionType,
              overflow_menu::ActionTypeForStringName(
                  overflow_menu::StringNameForActionType(actionType)));
  }
}

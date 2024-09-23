// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_metrics.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

OverflowMenuDestination* CreateOverflowMenuDestination(
    overflow_menu::Destination destination) {
  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:@"Foobar"
                                         symbolName:@""
                                       systemSymbol:YES
                                   monochromeSymbol:NO
                            accessibilityIdentifier:@"Foobar"
                                 enterpriseDisabled:NO
                                displayNewLabelIcon:NO
                                            handler:^{
                                                // Do nothing
                                            }];

  result.destination = static_cast<NSInteger>(destination);

  return result;
}

OverflowMenuAction* CreateOverflowMenuAction(
    overflow_menu::ActionType actionType) {
  OverflowMenuAction* result =
      [[OverflowMenuAction alloc] initWithName:@"Foobar"
                                    symbolName:@""
                                  systemSymbol:YES
                              monochromeSymbol:NO
                       accessibilityIdentifier:@"Foobar"
                            enterpriseDisabled:NO
                           displayNewLabelIcon:NO
                                       handler:^{
                                           // Do nothing
                                       }];

  result.actionType = static_cast<NSInteger>(actionType);

  return result;
}

}  // namespace

class OverflowMenuMetricsTest : public PlatformTest {};

// Test various combinations of inputs for ItemWasInitiallyVisible
TEST_F(OverflowMenuMetricsTest, ItemWasInitiallyVisible) {
  // Test happy path.
  EXPECT_TRUE(ItemWasInitiallyVisible(0, @[ @0, @1, @2, @3 ], 2));
  EXPECT_TRUE(ItemWasInitiallyVisible(1, @[ @0, @1, @2, @3 ], 2));
  EXPECT_FALSE(ItemWasInitiallyVisible(2, @[ @0, @1, @2, @3 ], 2));
  EXPECT_FALSE(ItemWasInitiallyVisible(3, @[ @0, @1, @2, @3 ], 2));

  // Test with array length less than numVisibleItems.
  EXPECT_TRUE(ItemWasInitiallyVisible(0, @[ @0, @1, @2, @3 ], 9));
  EXPECT_TRUE(ItemWasInitiallyVisible(1, @[ @0, @1, @2, @3 ], 9));
  EXPECT_TRUE(ItemWasInitiallyVisible(2, @[ @0, @1, @2, @3 ], 9));
  EXPECT_TRUE(ItemWasInitiallyVisible(3, @[ @0, @1, @2, @3 ], 9));
}

// Test that DestinationWasInitiallyVisible works correctly.
TEST_F(OverflowMenuMetricsTest, DestinationWasInitiallyVisible) {
  NSArray<OverflowMenuDestination*>* destinations = @[
    CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks),
    CreateOverflowMenuDestination(overflow_menu::Destination::History),
    CreateOverflowMenuDestination(overflow_menu::Destination::ReadingList),
  ];

  EXPECT_TRUE(DestinationWasInitiallyVisible(
      overflow_menu::Destination::Bookmarks, destinations, 2));
  EXPECT_TRUE(DestinationWasInitiallyVisible(
      overflow_menu::Destination::History, destinations, 2));
  EXPECT_FALSE(DestinationWasInitiallyVisible(
      overflow_menu::Destination::ReadingList, destinations, 2));
}

// Test that ActionWasInitiallyVisible works correctly.
TEST_F(OverflowMenuMetricsTest, ActionWasInitiallyVisible) {
  NSArray<OverflowMenuAction*>* actions = @[
    CreateOverflowMenuAction(overflow_menu::ActionType::Follow),
    CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark),
    CreateOverflowMenuAction(overflow_menu::ActionType::ReadingList),
  ];

  EXPECT_TRUE(
      ActionWasInitiallyVisible(overflow_menu::ActionType::Follow, actions, 2));
  EXPECT_TRUE(ActionWasInitiallyVisible(overflow_menu::ActionType::Bookmark,
                                        actions, 2));
  EXPECT_FALSE(ActionWasInitiallyVisible(overflow_menu::ActionType::ReadingList,
                                         actions, 2));
}

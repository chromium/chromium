// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

OverflowMenuDestination* CreateOverflowMenuDestination(
    overflow_menu::Destination destination) {
  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:@"Foobar"
                                         symbolName:kSettingsSymbol
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

}  // namespace

class DestinationCustomizationModelTest : public PlatformTest {
 public:
  DestinationCustomizationModelTest() {}

 protected:
  DestinationCustomizationModel* model_;
};

// Tests that hiding a destination moves it to the hidden group.
TEST_F(DestinationCustomizationModelTest, HidingDestinationMovesToHidden) {
  OverflowMenuDestination* bookmarks =
      CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks);
  OverflowMenuDestination* history =
      CreateOverflowMenuDestination(overflow_menu::Destination::History);
  model_ = [[DestinationCustomizationModel alloc]
         initWithDestinations:@[ bookmarks, history ]
      destinationUsageEnabled:YES];

  bookmarks.shown = NO;

  ASSERT_EQ(model_.shownDestinations.count, 1u);
  EXPECT_EQ(model_.shownDestinations[0], history);
  ASSERT_EQ(model_.hiddenDestinations.count, 1u);
  EXPECT_EQ(model_.hiddenDestinations[0], bookmarks);
}

// Tests that showing a hidden action moves it to the shown group.
TEST_F(DestinationCustomizationModelTest, ShowingMovesToShown) {
  OverflowMenuDestination* bookmarks =
      CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks);
  bookmarks.shown = NO;
  model_ =
      [[DestinationCustomizationModel alloc] initWithDestinations:@[ bookmarks ]
                                          destinationUsageEnabled:YES];

  bookmarks.shown = YES;

  ASSERT_EQ(model_.shownDestinations.count, 1u);
  EXPECT_EQ(model_.shownDestinations[0], bookmarks);
  ASSERT_EQ(model_.hiddenDestinations.count, 0u);
}

// Tests that newly hidden items are moved to the end of hidden array one at a
// time, in order.
TEST_F(DestinationCustomizationModelTest, HidingMovesToEnd) {
  OverflowMenuDestination* bookmarks =
      CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks);
  OverflowMenuDestination* history =
      CreateOverflowMenuDestination(overflow_menu::Destination::History);
  OverflowMenuDestination* settings =
      CreateOverflowMenuDestination(overflow_menu::Destination::Settings);
  model_ = [[DestinationCustomizationModel alloc]
         initWithDestinations:@[ bookmarks, history, settings ]
      destinationUsageEnabled:YES];

  // Hide history first, and then settings. History should be first in the
  // hidden list.
  history.shown = NO;
  settings.shown = NO;

  ASSERT_EQ(model_.shownDestinations.count, 1u);
  EXPECT_EQ(model_.shownDestinations[0], bookmarks);
  ASSERT_EQ(model_.hiddenDestinations.count, 2u);
  EXPECT_EQ(model_.hiddenDestinations[0], history);
  EXPECT_EQ(model_.hiddenDestinations[1], settings);
}

// Tests that newly shown items are moved to the front of shown array one at a
// time, in order.
TEST_F(DestinationCustomizationModelTest, ShowingMovesToFront) {
  OverflowMenuDestination* bookmarks =
      CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks);
  OverflowMenuDestination* history =
      CreateOverflowMenuDestination(overflow_menu::Destination::History);
  OverflowMenuDestination* settings =
      CreateOverflowMenuDestination(overflow_menu::Destination::Settings);

  history.shown = NO;
  settings.shown = NO;

  model_ = [[DestinationCustomizationModel alloc]
         initWithDestinations:@[ bookmarks, history, settings ]
      destinationUsageEnabled:YES];

  // Show history first, and then settings. Settings should end up first in the
  // shown list.
  history.shown = YES;
  settings.shown = YES;

  ASSERT_EQ(model_.shownDestinations.count, 3u);
  EXPECT_EQ(model_.shownDestinations[0], settings);
  EXPECT_EQ(model_.shownDestinations[1], history);
  EXPECT_EQ(model_.shownDestinations[2], bookmarks);
  ASSERT_EQ(model_.hiddenDestinations.count, 0u);
}

// Tests that the model partitions destinations into shown and hidden halves
// correctly.
TEST_F(DestinationCustomizationModelTest, ModelPartitionsDestinations) {
  OverflowMenuDestination* bookmarks =
      CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks);
  OverflowMenuDestination* history =
      CreateOverflowMenuDestination(overflow_menu::Destination::History);
  OverflowMenuDestination* settings =
      CreateOverflowMenuDestination(overflow_menu::Destination::Settings);
  OverflowMenuDestination* readingList =
      CreateOverflowMenuDestination(overflow_menu::Destination::ReadingList);

  settings.shown = NO;
  readingList.shown = NO;

  model_ = [[DestinationCustomizationModel alloc]
         initWithDestinations:@[ bookmarks, settings, history, readingList ]
      destinationUsageEnabled:YES];

  ASSERT_EQ(model_.shownDestinations.count, 2u);
  EXPECT_EQ(model_.shownDestinations[0], bookmarks);
  EXPECT_EQ(model_.shownDestinations[1], history);
  ASSERT_EQ(model_.hiddenDestinations.count, 2u);
  EXPECT_EQ(model_.hiddenDestinations[0], settings);
  EXPECT_EQ(model_.hiddenDestinations[1], readingList);
}

// Tests that if an item's shown is toggled, destination usage is turned off.
TEST_F(DestinationCustomizationModelTest, HidingItemTurnsOffDestinationUsage) {
  OverflowMenuDestination* bookmarks =
      CreateOverflowMenuDestination(overflow_menu::Destination::Bookmarks);
  OverflowMenuDestination* history =
      CreateOverflowMenuDestination(overflow_menu::Destination::History);
  model_ = [[DestinationCustomizationModel alloc]
         initWithDestinations:@[ bookmarks, history ]
      destinationUsageEnabled:YES];

  bookmarks.shown = NO;

  EXPECT_EQ(model_.destinationUsageEnabled, NO);
}

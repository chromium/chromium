// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_table_view_controller.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Tests the MagicStackHalfSheetTableViewController's core functionality.
class MagicStackHalfSheetTableViewControllerUnittest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {kSafetyCheckMagicStack, kTabResumption}, {});

    view_controller_ = [[MagicStackHalfSheetTableViewController alloc] init];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  MagicStackHalfSheetTableViewController* view_controller_;
};

// Tests that all of the module disable settings are configured correctly on
// initial load.
TEST_F(MagicStackHalfSheetTableViewControllerUnittest, TestLoadModel) {
  [view_controller_ showSetUpList:YES];
  [view_controller_ setSetUpListDisabled:NO];
  [view_controller_ setSafetyCheckDisabled:NO];
  [view_controller_ setTabResumptionDisabled:NO];
  [view_controller_ setParcelTrackingDisabled:NO];

  // Parcel tracking is only enabled in the US.
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  [view_controller_ loadViewIfNeeded];

  TableViewModel* model = view_controller_.tableViewModel;
  ASSERT_TRUE([model numberOfItemsInSection:0] == 4);

  TableViewSwitchItem* setUpListItem = static_cast<TableViewSwitchItem*>(
      [model itemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]]);
  EXPECT_TRUE(setUpListItem.on);

  TableViewSwitchItem* safetyCheckItem = static_cast<TableViewSwitchItem*>(
      [model itemAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]]);
  EXPECT_TRUE(safetyCheckItem.on);

  TableViewSwitchItem* tabResumptionItem = static_cast<TableViewSwitchItem*>(
      [model itemAtIndexPath:[NSIndexPath indexPathForItem:2 inSection:0]]);
  EXPECT_TRUE(tabResumptionItem.on);

  TableViewSwitchItem* parcelTrackingItem = static_cast<TableViewSwitchItem*>(
      [model itemAtIndexPath:[NSIndexPath indexPathForItem:3 inSection:0]]);
  EXPECT_TRUE(parcelTrackingItem.on);
}

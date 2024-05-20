// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_view_controller.h"

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "testing/platform_test.h"

// Tests the ParcelTrackingSettingsViewController's core functionality.
class ParcelTrackingSettingsViewControllerUnittest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    view_controller_ = [[ParcelTrackingSettingsViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }

 protected:
  ParcelTrackingSettingsViewController* view_controller_;
};

// Tests that the correct item has a checkmark for the initial status.
TEST_F(ParcelTrackingSettingsViewControllerUnittest, TestLoadModel) {
  [view_controller_
      updateCheckedState:IOSParcelTrackingOptInStatus::kAskToTrack];
  [view_controller_ loadViewIfNeeded];

  TableViewModel* model = view_controller_.tableViewModel;

  ASSERT_TRUE([model numberOfItemsInSection:0] == 3);

  TableViewDetailTextItem* autoTrackItem =
      static_cast<TableViewDetailTextItem*>(
          [model itemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]]);
  EXPECT_EQ(autoTrackItem.accessoryType, UITableViewCellAccessoryNone);

  TableViewDetailTextItem* askEveryTimeItem =
      static_cast<TableViewDetailTextItem*>(
          [model itemAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]]);
  EXPECT_EQ(askEveryTimeItem.accessoryType, UITableViewCellAccessoryCheckmark);

  TableViewDetailTextItem* neverTrackItem =
      static_cast<TableViewDetailTextItem*>(
          [model itemAtIndexPath:[NSIndexPath indexPathForItem:2 inSection:0]]);
  EXPECT_EQ(neverTrackItem.accessoryType, UITableViewCellAccessoryNone);
}

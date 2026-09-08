// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/ui/site_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SiteSettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    delegate_ = OCMStrictProtocolMock(
        @protocol(SiteSettingsTableViewControllerDelegate));
  }

  LegacyChromeTableViewController* InstantiateController() override {
    SiteSettingsTableViewController* controller =
        [[SiteSettingsTableViewController alloc] init];
    controller.delegate = delegate_;
    return controller;
  }

  SiteSettingsTableViewController* GetSiteSettingsTableViewController() {
    return base::apple::ObjCCastStrict<SiteSettingsTableViewController>(
        controller());
  }

  id delegate_;
};

// Tests categories setup, dynamic location insertion, and subtitle updates.
TEST_F(SiteSettingsTableViewControllerTest, TestCategoriesAndSubtitles) {
  CreateController();
  CheckController();

  SiteSettingsTableViewController* controller =
      GetSiteSettingsTableViewController();
  EXPECT_NSEQ(@"Site settings", controller.title);
  EXPECT_NSEQ(kSiteSettingsTableViewId,
              controller.tableView.accessibilityIdentifier);

  // By default, location is not enabled.
  EXPECT_EQ(1, [controller.tableViewModel numberOfSections]);
  EXPECT_EQ(2, [controller.tableViewModel numberOfItemsInSection:0]);

  NSIndexPath* micIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  TableViewDetailIconItem* micItem =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          [controller.tableViewModel itemAtIndexPath:micIndexPath]);
  EXPECT_NSEQ(kSiteSettingsMicrophoneCellId, micItem.accessibilityIdentifier);

  NSIndexPath* cameraIndexPath = [NSIndexPath indexPathForRow:1 inSection:0];
  TableViewDetailIconItem* cameraItem =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          [controller.tableViewModel itemAtIndexPath:cameraIndexPath]);
  EXPECT_NSEQ(kSiteSettingsCameraCellId, cameraItem.accessibilityIdentifier);

  // Enable location and verify it appears.
  [controller setLocationCategoryEnabled:YES];
  EXPECT_EQ(3, [controller.tableViewModel numberOfItemsInSection:0]);

  NSIndexPath* locationIndexPath = [NSIndexPath indexPathForRow:2 inSection:0];
  TableViewDetailIconItem* locationItem =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          [controller.tableViewModel itemAtIndexPath:locationIndexPath]);
  EXPECT_NSEQ(kSiteSettingsLocationCellId,
              locationItem.accessibilityIdentifier);

  // Update default settings and verify subtitles.
  [controller setDefaultSetting:CONTENT_SETTING_ASK
                        forType:ContentSettingsType::MEDIASTREAM_MIC];
  [controller setDefaultSetting:CONTENT_SETTING_BLOCK
                        forType:ContentSettingsType::MEDIASTREAM_CAMERA];
  [controller setDefaultSetting:CONTENT_SETTING_ASK
                        forType:ContentSettingsType::GEOLOCATION];

  EXPECT_NSEQ(@"Ask first", micItem.detailText);
  EXPECT_NSEQ(@"Not allowed", cameraItem.detailText);
  EXPECT_NSEQ(@"Ask first", locationItem.detailText);

  // Disable location and verify it is removed.
  [controller setLocationCategoryEnabled:NO];
  EXPECT_EQ(2, [controller.tableViewModel numberOfItemsInSection:0]);
}

// Tests that selecting each permission row notifies the delegate with the
// corresponding content settings type.
TEST_F(SiteSettingsTableViewControllerTest, TestRowSelection) {
  CreateController();
  CheckController();

  SiteSettingsTableViewController* controller =
      GetSiteSettingsTableViewController();
  [controller setLocationCategoryEnabled:YES];

  OCMExpect([delegate_
      siteSettingsTableViewController:controller
                 didSelectSettingType:ContentSettingsType::MEDIASTREAM_MIC]);
  [controller.tableView.delegate tableView:controller.tableView
      performPrimaryActionForRowAtIndexPath:[NSIndexPath indexPathForRow:0
                                                               inSection:0]];
  EXPECT_OCMOCK_VERIFY(delegate_);

  OCMExpect([delegate_
      siteSettingsTableViewController:controller
                 didSelectSettingType:ContentSettingsType::MEDIASTREAM_CAMERA]);
  [controller.tableView.delegate tableView:controller.tableView
      performPrimaryActionForRowAtIndexPath:[NSIndexPath indexPathForRow:1
                                                               inSection:0]];
  EXPECT_OCMOCK_VERIFY(delegate_);

  OCMExpect([delegate_
      siteSettingsTableViewController:controller
                 didSelectSettingType:ContentSettingsType::GEOLOCATION]);
  [controller.tableView.delegate tableView:controller.tableView
      performPrimaryActionForRowAtIndexPath:[NSIndexPath indexPathForRow:2
                                                               inSection:0]];
  EXPECT_OCMOCK_VERIFY(delegate_);
}

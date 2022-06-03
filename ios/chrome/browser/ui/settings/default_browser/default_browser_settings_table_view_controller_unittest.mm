// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests the items shown in DefaultBrowserSettingTableViewController.
class DefaultBrowserSettingTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  DefaultBrowserSettingTableViewControllerTest() {}

  void SetUp() override { ChromeTableViewControllerTest::SetUp(); }

  ChromeTableViewController* InstantiateController() override {
    return [[DefaultBrowserSettingsTableViewController alloc] init];
  }
};

TEST_F(DefaultBrowserSettingTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();

  CheckTitleWithId(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  EXPECT_EQ(1, NumberOfItemsInSection(1));

  CheckSectionHeaderWithId(IDS_IOS_SETTINGS_HEADER_TEXT, 0);
}

}  // namespace

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/platform_test.h"

namespace {

// Tests the items shown in DefaultBrowserSettingTableViewController.
class DefaultBrowserSettingsTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  DefaultBrowserSettingsTableViewControllerTest() {}

  void SetUp() override { ChromeTableViewControllerTest::SetUp(); }

  void TearDown() override {
    [base::apple::ObjCCastStrict<DefaultBrowserSettingsTableViewController>(
        controller()) settingsWillBeDismissed];
    ChromeTableViewControllerTest::TearDown();
  }
  ChromeTableViewController* InstantiateController() override {
    return [[DefaultBrowserSettingsTableViewController alloc] init];
  }
};

TEST_F(DefaultBrowserSettingsTableViewControllerTest, TestModel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {kDefaultBrowserVideoInSettings});
  CreateController();
  CheckController();

  CheckTitleWithId(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);

  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(0, NumberOfItemsInSection(0));
  EXPECT_EQ(3, NumberOfItemsInSection(1));
  EXPECT_EQ(1, NumberOfItemsInSection(2));

  CheckSectionHeaderWithId(IDS_IOS_SETTINGS_HEADER_TEXT, 0);
  CheckSectionHeaderWithId(IDS_IOS_SETTINGS_FOLLOW_STEPS_BELOW_TEXT, 1);
}

TEST_F(DefaultBrowserSettingsTableViewControllerTest,
       TestDefaultBrowserInstructionsView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kDefaultBrowserVideoInSettings}, {});

  CreateController();

  CheckTitleWithId(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);

  EXPECT_EQ(0, NumberOfSections());
}
}  // namespace

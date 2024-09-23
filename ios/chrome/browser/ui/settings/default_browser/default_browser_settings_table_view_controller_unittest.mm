// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/platform_test.h"

namespace {

// Tests the items shown in DefaultBrowserSettingTableViewController.
class DefaultBrowserSettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  DefaultBrowserSettingsTableViewControllerTest() {}

  void SetUp() override { LegacyChromeTableViewControllerTest::SetUp(); }

  void TearDown() override {
    [base::apple::ObjCCastStrict<DefaultBrowserSettingsTableViewController>(
        controller()) settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }
  LegacyChromeTableViewController* InstantiateController() override {
    return [[DefaultBrowserSettingsTableViewController alloc] init];
  }
};

TEST_F(DefaultBrowserSettingsTableViewControllerTest,
       TestDefaultBrowserInstructionsView) {
  CreateController();

  CheckTitleWithId(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);

  EXPECT_EQ(0, NumberOfSections());
}
}  // namespace

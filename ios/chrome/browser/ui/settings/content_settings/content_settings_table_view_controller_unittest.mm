// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/content_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class ContentSettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  ContentSettingsTableViewControllerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<ContentSettingsTableViewController>(
        controller()) settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[ContentSettingsTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that the number of items in Content Settings is correct.
TEST_F(ContentSettingsTableViewControllerTest,
       TestModelWithLanguageSettingsUI) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_CONTENT_SETTINGS_TITLE);

  if (web::features::IsWebInspectorSupportEnabled()) {
    ASSERT_EQ(2, NumberOfSections());
    ASSERT_EQ(1, NumberOfItemsInSection(1));
  } else {
    ASSERT_EQ(1, NumberOfSections());
  }
  if (base::FeatureList::IsEnabled(web::features::kEnableMeasurements)) {
    ASSERT_EQ(5, NumberOfItemsInSection(0));
  } else {
    ASSERT_EQ(4, NumberOfItemsInSection(0));
  }
  CheckDetailItemTextWithIds(IDS_IOS_BLOCK_POPUPS, IDS_IOS_SETTING_ON, 0, 0);
}

}  // namespace

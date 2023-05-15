// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/content_settings_table_view_controller.h"

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ContentSettingsTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  ContentSettingsTableViewControllerTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  }

  ChromeTableViewController* InstantiateController() override {
    return [[ContentSettingsTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
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
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  CheckDetailItemTextWithIds(IDS_IOS_BLOCK_POPUPS, IDS_IOS_SETTING_ON, 0, 0);
}

}  // namespace

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/content_settings/reader_mode_settings_table_view_controller.h"

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"

namespace {

class ReaderModeSettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  ReaderModeSettingsTableViewControllerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    distilled_page_prefs_ = std::make_unique<dom_distiller::DistilledPagePrefs>(
        profile_->GetPrefs());
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[ReaderModeSettingsTableViewController alloc]
        initWithDistilledPagePrefs:distilled_page_prefs_.get()
                       prefService:profile_->GetPrefs()];
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs_;
};

// Tests that the Reading Mode settings model is correctly loaded.
TEST_F(ReaderModeSettingsTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_READER_MODE_CONTENT_SETTINGS_TITLE);

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));
  ASSERT_EQ(1, NumberOfItemsInSection(1));
}

}  // namespace

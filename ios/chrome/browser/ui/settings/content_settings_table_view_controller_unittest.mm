// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings_table_view_controller.h"

#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ContentSettingsTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[ContentSettingsTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  PrefService* GetPrefs() { return chrome_browser_state_->GetPrefs(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests that there are 3 items in Content Settings if kLanguageSettings feature
// is disabled.
TEST_F(ContentSettingsTableViewControllerTest,
       TestModelWithoutLanguageSettingsUI) {
  // Disable the Language Settings UI.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {kLanguageSettings});

  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_CONTENT_SETTINGS_TITLE);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  CheckDetailItemTextWithIds(IDS_IOS_BLOCK_POPUPS, IDS_IOS_SETTING_ON, 0, 0);
  CheckDetailItemTextWithIds(IDS_IOS_TRANSLATE_SETTING, IDS_IOS_SETTING_ON, 0,
                             1);
}

// Tests that there are 2 items in Content Settings if kLanguageSettings feature
// is enabled.
TEST_F(ContentSettingsTableViewControllerTest,
       TestModelWithLanguageSettingsUI) {
  // Enable the Language Settings UI.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kLanguageSettings}, {});

  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_CONTENT_SETTINGS_TITLE);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckDetailItemTextWithIds(IDS_IOS_BLOCK_POPUPS, IDS_IOS_SETTING_ON, 0, 0);
}

TEST_F(ContentSettingsTableViewControllerTest, TestOnPreferenceChanged) {
  // Enable the Language Settings UI.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kLanguageSettings}, {});

  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_CONTENT_SETTINGS_TITLE);

  // TODO(crbug.com/1008433): This test can be improved by checking the state
  // to verify that -onPreferenceChanged: is indeed called. However, this
  // test will become irrelevant once kLanguageSettings feature flag is removed.
  GetPrefs()->SetBoolean(prefs::kOfferTranslateEnabled, true);
  GetPrefs()->SetBoolean(prefs::kOfferTranslateEnabled, false);
}

}  // namespace

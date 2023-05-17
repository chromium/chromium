// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_view_controller.h"

#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_mediator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PrivacySafeBrowsingViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  // Makes a PrefService to be used by the test.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    user_prefs::PrefRegistrySyncable* registry = prefs->registry();
    RegisterBrowserStatePrefs(registry);
    return prefs;
  }

  ChromeTableViewController* InstantiateController() override {
    mediator_ = [[PrivacySafeBrowsingMediator alloc]
        initWithUserPrefService:chrome_browser_state_->GetPrefs()];
    PrivacySafeBrowsingViewController* view_controller =
        [[PrivacySafeBrowsingViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
    mediator_.consumer = view_controller;
    return view_controller;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  PrivacySafeBrowsingMediator* mediator_;
};

TEST_F(PrivacySafeBrowsingViewControllerTest, TestModel) {
  CreateController();
  CheckController();

  CheckTitle(l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE));
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_SUMMARY),
      0, 0);
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_SUMMARY),
      0, 1);
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_SUMMARY),
      0, 2);
}

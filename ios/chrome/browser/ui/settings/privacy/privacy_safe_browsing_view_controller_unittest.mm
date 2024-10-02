// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_view_controller.h"

#import "build/branding_buildflags.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_mediator.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ui/base/l10n/l10n_util.h"

class PrivacySafeBrowsingViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    profile_ = std::move(builder).Build();
  }

  // Makes a PrefService to be used by the test.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    user_prefs::PrefRegistrySyncable* registry = prefs->registry();
    RegisterProfilePrefs(registry);
    return prefs;
  }

  LegacyChromeTableViewController* InstantiateController() override {
    mediator_ = [[PrivacySafeBrowsingMediator alloc]
        initWithUserPrefService:profile_->GetPrefs()];
    PrivacySafeBrowsingViewController* view_controller =
        [[PrivacySafeBrowsingViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
    mediator_.consumer = view_controller;
    return view_controller;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
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
          IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_FRIENDLIER_SUMMARY),
      0, 0);

  NSInteger standard_protection_summary = 0;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (safe_browsing::hash_realtime_utils::
          IsHashRealTimeLookupEligibleInSession()) {
    standard_protection_summary =
        IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_FRIENDLIER_SUMMARY_PROXY;
  }
#endif

  if (!standard_protection_summary) {
    standard_protection_summary =
        IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_FRIENDLIER_SUMMARY;
  }

  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE),
      l10n_util::GetNSString(standard_protection_summary), 0, 1);
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_FRIENDLIER_SUMMARY),
      0, 2);
}

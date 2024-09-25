// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"

#import "base/files/file_path.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class BrowserPrefsTest : public PlatformTest {
 protected:
  BrowserPrefsTest() {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state());

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterProfilePrefs(pref_service_->registry());

    // TODO(crbug.com/40282890): Remove this line ~one year after full launch.
    // Manually register IdentityManagerFactory preferences as ProfilePrefs do
    // not register KeyedService factories prefs.
    signin::IdentityManager::RegisterProfilePrefs(pref_service_->registry());
  }

  void TearDown() override {
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    PlatformTest::TearDown();
  }

  PrefService* pref_service() { return pref_service_.get(); }
  PrefService* local_state() { return local_state_.get(); }

 private:
  // Application pref service.
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  // Profile pref service.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
};

// Check that the migration of a pref from profile prefService to
// localState prefService is performed correctly.
TEST_F(BrowserPrefsTest, VerifyProfilePrefsMigration) {
  base::Time now = base::Time::Now();

  // Simulate registering a value different from default in profile prefService.
  pref_service()->SetBoolean(prefs::kBottomOmnibox, true);
  pref_service()->SetBoolean(prefs::kBottomOmniboxByDefault, true);
  pref_service()->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, true);
  pref_service()->SetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime,
                          now);
  pref_service()->SetInteger(prefs::kIdentityConfirmationSnackbarDisplayCount,
                             1);
  pref_service()->SetBoolean(prefs::kIncognitoInterstitialEnabled, true);
  pref_service()->SetInteger(prefs::kAddressBarSettingsNewBadgeShownCount, 1);

  EXPECT_EQ(pref_service()->GetBoolean(prefs::kBottomOmnibox), true);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmnibox), false);

  EXPECT_EQ(pref_service()->GetBoolean(prefs::kBottomOmniboxByDefault), true);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmniboxByDefault), false);

  EXPECT_EQ(pref_service()->GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            true);
  EXPECT_EQ(local_state()->GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            false);

  EXPECT_EQ(pref_service()->GetTime(
                prefs::kIdentityConfirmationSnackbarLastPromptTime),
            now);
  EXPECT_EQ(local_state()->GetTime(
                prefs::kIdentityConfirmationSnackbarLastPromptTime),
            base::Time());

  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            1);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            0);

  EXPECT_EQ(pref_service()->GetBoolean(prefs::kIncognitoInterstitialEnabled),
            true);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kIncognitoInterstitialEnabled),
            false);

  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      1);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      0);

  MigrateObsoleteProfilePrefs(base::FilePath(), pref_service());

  // Verify that the prefs were migrated successfully.
  EXPECT_EQ(pref_service()->GetBoolean(prefs::kBottomOmnibox), false);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmnibox), true);

  EXPECT_EQ(pref_service()->GetBoolean(prefs::kBottomOmniboxByDefault), false);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmniboxByDefault), true);

  EXPECT_EQ(pref_service()->GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            false);
  EXPECT_EQ(local_state()->GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            true);

  EXPECT_EQ(pref_service()->GetTime(
                prefs::kIdentityConfirmationSnackbarLastPromptTime),
            base::Time());
  EXPECT_EQ(local_state()->GetTime(
                prefs::kIdentityConfirmationSnackbarLastPromptTime),
            now);

  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            0);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            1);

  EXPECT_EQ(pref_service()->GetBoolean(prefs::kIncognitoInterstitialEnabled),
            false);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kIncognitoInterstitialEnabled),
            true);

  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      0);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      1);
}

// Check that the migration of a pref from localState prefService to
// profile prefService is performed correctly.
TEST_F(BrowserPrefsTest, VerifyLocalStatePrefsMigration) {
  base::Value::List list_example = base::Value::List().Append("Example");
  base::Value::Dict dict_example;
  dict_example.Set("Example_key", "Example_value");

  // Simulate registering a value different from default in localState
  // prefService.
  local_state()->SetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount, 10);
  local_state()->SetList(prefs::kIosLatestMostVisitedSites,
                         list_example.Clone());
  local_state()->SetString(prefs::kIosSafetyCheckManagerPasswordCheckResult,
                           "Example");
  local_state()->SetString(
      tab_resumption_prefs::kTabResumptionLastOpenedTabURLPref, "Example");
  local_state()->SetDict(prefs::kIosPreRestoreAccountInfo,
                         dict_example.Clone());

  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      0);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      10);

  EXPECT_EQ(pref_service()->GetList(prefs::kIosLatestMostVisitedSites),
            base::Value::List());
  EXPECT_EQ(local_state()->GetList(prefs::kIosLatestMostVisitedSites),
            list_example);

  EXPECT_EQ(pref_service()->GetString(
                prefs::kIosSafetyCheckManagerPasswordCheckResult),
            NameForSafetyCheckState(PasswordSafetyCheckState::kDefault));
  EXPECT_EQ(local_state()->GetString(
                prefs::kIosSafetyCheckManagerPasswordCheckResult),
            "Example");

  EXPECT_EQ(pref_service()->GetString(
                tab_resumption_prefs::kTabResumptionLastOpenedTabURLPref),
            std::string());
  EXPECT_EQ(local_state()->GetString(
                tab_resumption_prefs::kTabResumptionLastOpenedTabURLPref),
            "Example");

  EXPECT_EQ(pref_service()->GetDict(prefs::kIosPreRestoreAccountInfo).size(),
            0ul);
  EXPECT_EQ(local_state()->GetDict(prefs::kIosPreRestoreAccountInfo),
            dict_example);

  MigrateObsoleteProfilePrefs(base::FilePath(), pref_service());

  // Verify that the prefs were migrated successfully.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      10);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      0);

  EXPECT_EQ(pref_service()->GetList(prefs::kIosLatestMostVisitedSites),
            list_example);
  EXPECT_EQ(local_state()->GetList(prefs::kIosLatestMostVisitedSites),
            base::Value::List());

  EXPECT_EQ(pref_service()->GetString(
                prefs::kIosSafetyCheckManagerPasswordCheckResult),
            "Example");
  EXPECT_EQ(local_state()->GetString(
                prefs::kIosSafetyCheckManagerPasswordCheckResult),
            NameForSafetyCheckState(PasswordSafetyCheckState::kDefault));

  EXPECT_EQ(pref_service()->GetString(
                tab_resumption_prefs::kTabResumptionLastOpenedTabURLPref),
            "Example");
  EXPECT_EQ(local_state()->GetString(
                tab_resumption_prefs::kTabResumptionLastOpenedTabURLPref),
            std::string());

  EXPECT_EQ(pref_service()->GetDict(prefs::kIosPreRestoreAccountInfo),
            dict_example);
  EXPECT_EQ(local_state()->GetDict(prefs::kIosPreRestoreAccountInfo).size(),
            0ul);
}

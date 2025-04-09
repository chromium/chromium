// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"

#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class BrowserPrefsTest : public PlatformTest {
 protected:
  BrowserPrefsTest() {
    RegisterProfilePrefs(pref_service_.registry());

    // TODO(crbug.com/40282890): Remove this line ~one year after full launch.
    // Manually register IdentityManagerFactory preferences as ProfilePrefs do
    // not register KeyedService factories prefs.
    signin::IdentityManager::RegisterProfilePrefs(pref_service_.registry());
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  // Application pref service.
  IOSChromeScopedTestingLocalState local_state_;
  // Profile pref service.
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// Check that the migration of a pref from profile prefService to
// localState prefService is performed correctly.
TEST_F(BrowserPrefsTest, VerifyProfilePrefsMigration) {
  base::Time now = base::Time::Now();

  // Simulate registering a value different from default in profile prefService.
  pref_service_.SetBoolean(prefs::kBottomOmnibox, true);
  pref_service_.SetBoolean(prefs::kBottomOmniboxByDefault, true);
  pref_service_.SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, true);
  pref_service_.SetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime,
                        now);
  pref_service_.SetInteger(prefs::kIdentityConfirmationSnackbarDisplayCount, 1);
  pref_service_.SetBoolean(prefs::kIncognitoInterstitialEnabled, true);
  pref_service_.SetInteger(prefs::kAddressBarSettingsNewBadgeShownCount, 1);

  EXPECT_EQ(pref_service_.GetBoolean(prefs::kBottomOmnibox), true);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmnibox), false);

  EXPECT_EQ(pref_service_.GetBoolean(prefs::kBottomOmniboxByDefault), true);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmniboxByDefault), false);

  EXPECT_EQ(pref_service_.GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            true);
  EXPECT_EQ(local_state()->GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            false);

  EXPECT_EQ(
      pref_service_.GetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime),
      now);
  EXPECT_EQ(local_state()->GetTime(
                prefs::kIdentityConfirmationSnackbarLastPromptTime),
            base::Time());

  EXPECT_EQ(pref_service_.GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            1);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            0);

  EXPECT_EQ(pref_service_.GetBoolean(prefs::kIncognitoInterstitialEnabled),
            true);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kIncognitoInterstitialEnabled),
            false);

  EXPECT_EQ(
      pref_service_.GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      1);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      0);

  MigrateObsoleteProfilePrefs(&pref_service_);

  // Verify that the prefs were migrated successfully.
  EXPECT_EQ(pref_service_.GetBoolean(prefs::kBottomOmnibox), false);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmnibox), true);

  EXPECT_EQ(pref_service_.GetBoolean(prefs::kBottomOmniboxByDefault), false);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kBottomOmniboxByDefault), true);

  EXPECT_EQ(pref_service_.GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            false);
  EXPECT_EQ(local_state()->GetBoolean(
                password_manager::prefs::kCredentialProviderEnabledOnStartup),
            true);

  EXPECT_EQ(
      pref_service_.GetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime),
      base::Time());
  EXPECT_EQ(local_state()->GetTime(
                prefs::kIdentityConfirmationSnackbarLastPromptTime),
            now);

  EXPECT_EQ(pref_service_.GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            0);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kIdentityConfirmationSnackbarDisplayCount),
            1);

  EXPECT_EQ(pref_service_.GetBoolean(prefs::kIncognitoInterstitialEnabled),
            false);
  EXPECT_EQ(local_state()->GetBoolean(prefs::kIncognitoInterstitialEnabled),
            true);

  EXPECT_EQ(
      pref_service_.GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      0);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount),
      1);
}

// Check that the migration of a pref from localState prefService to
// profile prefService is performed correctly.
TEST_F(BrowserPrefsTest, VerifyLocalStatePrefsMigration) {
  // Setup test data
  base::Value::List list_example = base::Value::List().Append("Example");
  base::Value::Dict dict_example;
  dict_example.Set("Example_key", "Example_value");

  // Set initial values in local_state

  // New Tab Page Display Count
  local_state()->SetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount, 10);

  // Safety Check Manager and Settings
  local_state()->SetString(prefs::kIosSafetyCheckManagerPasswordCheckResult,
                           "Example");
  local_state()->SetBoolean(
      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref, true);

  // Account Info
  local_state()->SetDict(prefs::kIosPreRestoreAccountInfo,
                         dict_example.Clone());

  // Tab Resumption Settings
  local_state()->SetBoolean(tab_resumption_prefs::kTabResumptionDisabledPref,
                            true);

  // Magic Stack Segmentation Impressions
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, 5);
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness, 3);
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness, 7);
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
      2);
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness,
      4);
  local_state()->SetInteger(
      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount, 6);

  // Verify initial state before migration

  // Check New Tab Page Display Count
  EXPECT_EQ(
      pref_service_.GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      0);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      10);

  // Check Safety Check Manager and Settings
  EXPECT_EQ(
      pref_service_.GetString(prefs::kIosSafetyCheckManagerPasswordCheckResult),
      NameForSafetyCheckState(PasswordSafetyCheckState::kDefault));
  EXPECT_EQ(local_state()->GetString(
                prefs::kIosSafetyCheckManagerPasswordCheckResult),
            "Example");
  EXPECT_FALSE(pref_service_.GetBoolean(
      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref));
  EXPECT_TRUE(local_state()->GetBoolean(
      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref));

  // Check Account Info
  EXPECT_EQ(pref_service_.GetDict(prefs::kIosPreRestoreAccountInfo).size(),
            0ul);
  EXPECT_EQ(local_state()->GetDict(prefs::kIosPreRestoreAccountInfo),
            dict_example);

  // Check Tab Resumption Settings
  EXPECT_FALSE(pref_service_.GetBoolean(
      tab_resumption_prefs::kTabResumptionDisabledPref));
  EXPECT_TRUE(local_state()->GetBoolean(
      tab_resumption_prefs::kTabResumptionDisabledPref));

  // Check Magic Stack Segmentation Impressions in pref_service (should be -1)
  EXPECT_EQ(pref_service_.GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness),
            -1);
  EXPECT_EQ(
      pref_service_.GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness),
      -1);
  EXPECT_EQ(
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness),
      -1);
  EXPECT_EQ(
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness),
      -1);
  EXPECT_EQ(pref_service_.GetInteger(
                prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount),
            0);

  // Check Magic Stack Segmentation Impressions in local_state
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness),
            5);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness),
      3);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness),
      7);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness),
      2);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness),
      4);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount),
            6);

  // Perform migration
  MigrateObsoleteLocalStatePrefs(local_state());
  MigrateObsoleteProfilePrefs(&pref_service_);

  // Verify state after migration

  // Check New Tab Page Display Count
  EXPECT_EQ(
      pref_service_.GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      10);
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount),
      0);

  // Check Safety Check Manager and Settings
  EXPECT_EQ(
      pref_service_.GetString(prefs::kIosSafetyCheckManagerPasswordCheckResult),
      "Example");
  EXPECT_EQ(local_state()->GetString(
                prefs::kIosSafetyCheckManagerPasswordCheckResult),
            NameForSafetyCheckState(PasswordSafetyCheckState::kDefault));
  EXPECT_TRUE(pref_service_.GetBoolean(
      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref));
  EXPECT_FALSE(local_state()->GetBoolean(
      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref));

  // Check Account Info
  EXPECT_EQ(pref_service_.GetDict(prefs::kIosPreRestoreAccountInfo),
            dict_example);
  EXPECT_EQ(local_state()->GetDict(prefs::kIosPreRestoreAccountInfo).size(),
            0ul);

  // Check Tab Resumption Settings
  EXPECT_TRUE(pref_service_.GetBoolean(
      tab_resumption_prefs::kTabResumptionDisabledPref));
  EXPECT_FALSE(local_state()->GetBoolean(
      tab_resumption_prefs::kTabResumptionDisabledPref));

  // Check Magic Stack Segmentation Impressions in pref_service
  EXPECT_EQ(pref_service_.GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness),
            5);
  EXPECT_EQ(
      pref_service_.GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness),
      3);
  EXPECT_EQ(
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness),
      7);
  EXPECT_EQ(
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness),
      2);
  EXPECT_EQ(pref_service_.GetInteger(
                prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount),
            6);

  // Check Magic Stack Segmentation Impressions in local_state (should be -1)
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness),
            -1);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness),
      -1);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness),
      -1);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness),
      -1);
  EXPECT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness),
      -1);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount),
            0);
}

TEST_F(BrowserPrefsTest, VerifyUserDefaultsToProfilePrefsMigration) {
  NSString* kSyncDisabledAlertShownKey = @"SyncDisabledAlertShown";

  // Sets the value to migrate.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES forKey:kSyncDisabledAlertShownKey];
  ASSERT_TRUE([defaults boolForKey:kSyncDisabledAlertShownKey]);

  EXPECT_FALSE(
      pref_service_.GetBoolean(policy::policy_prefs::kSyncDisabledAlertShown));

  auto* sync_disabled_alert_shown_pref = pref_service_.FindPreference(
      policy::policy_prefs::kSyncDisabledAlertShown);
  ASSERT_TRUE(sync_disabled_alert_shown_pref);
  ASSERT_TRUE(sync_disabled_alert_shown_pref->IsDefaultValue());

  // Perform migration.
  MigrateObsoleteProfilePrefs(&pref_service_);

  // Verify migration.
  ASSERT_FALSE(sync_disabled_alert_shown_pref->IsDefaultValue());
  ASSERT_FALSE([defaults boolForKey:kSyncDisabledAlertShownKey]);
  EXPECT_TRUE(
      pref_service_.GetBoolean(policy::policy_prefs::kSyncDisabledAlertShown));

  // Perform migration again.
  MigrateObsoleteProfilePrefs(&pref_service_);

  ASSERT_FALSE(sync_disabled_alert_shown_pref->IsDefaultValue());
  ASSERT_FALSE([defaults boolForKey:kSyncDisabledAlertShownKey]);
  EXPECT_TRUE(
      pref_service_.GetBoolean(policy::policy_prefs::kSyncDisabledAlertShown));
}

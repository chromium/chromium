// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"

#import <string_view>

#import "components/commerce/core/pref_names.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/safety_check/safety_check_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/model/safety_check_prefs.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for pref registrations and migrations.
//
// The tests in this file are organized into the following categories:
// [1] Local-state to Profile pref migrations (triggered by
// `MigrateObsoleteProfilePrefs()`).
// [2] Profile to local-state pref migrations (triggered by
// `MigrateObsoleteProfilePrefs()`).
// [3] Profile pref renaming (triggered by `MigrateObsoleteProfilePrefs()`).
// [4] `NSUserDefaults` migrations (triggered by
// `MigrateObsoleteProfilePrefs()`).
// [5] Local-state pref migrations and cleanup (triggered by
// `MigrateObsoleteLocalStatePrefs()`).
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

  PrefService* profile_prefs() { return &pref_service_; }

 protected:
  // Local-state prefs.
  IOSChromeScopedTestingLocalState local_state_;
  // Profile prefs.
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// [1] Local-state to Profile pref migrations (triggered by
// `MigrateObsoleteProfilePrefs()`).

TEST_F(BrowserPrefsTest, MigrateMVTImpressionsFromLocalToProfile) {
  const int test_value = 5;

  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness,
      test_value);

  ASSERT_EQ(local_state()->GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness),
            test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(
              prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(
      local_state()
          ->FindPreference(
              prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness)
          ->IsDefaultValue());
  EXPECT_EQ(profile_prefs()->GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness),
            test_value);
}

TEST_F(BrowserPrefsTest, MigrateShortcutsImpressionsFromLocalToProfile) {
  const int test_value = 3;

  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
      test_value);

  ASSERT_EQ(
      local_state()->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness),
      test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(
      local_state()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness)
          ->IsDefaultValue());
  EXPECT_EQ(
      profile_prefs()->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness),
      test_value);
}

TEST_F(BrowserPrefsTest, MigrateSafetyCheckImpressionsFromLocalToProfile) {
  const int test_value = 7;

  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
      test_value);

  ASSERT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness),
      test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(
      local_state()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness)
          ->IsDefaultValue());
  EXPECT_EQ(
      profile_prefs()->GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness),
      test_value);
}

TEST_F(BrowserPrefsTest, MigrateTabResumptionImpressionsFromLocalToProfile) {
  const int test_value = 2;

  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
      test_value);

  ASSERT_EQ(
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness),
      test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(
      local_state()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness)
          ->IsDefaultValue());
  EXPECT_EQ(
      profile_prefs()->GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness),
      test_value);
}

TEST_F(BrowserPrefsTest, MigrateSafetyCheckIssuesCountFromLocalToProfile) {
  const int test_value = 6;

  local_state()->SetInteger(
      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount, test_value);

  ASSERT_EQ(local_state()->GetInteger(
                prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount),
            test_value);
  ASSERT_TRUE(profile_prefs()
                  ->FindPreference(
                      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount)
                  ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(local_state()
                  ->FindPreference(
                      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount)
                  ->IsDefaultValue());
  EXPECT_EQ(profile_prefs()->GetInteger(
                prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount),
            test_value);
}

// [2] Profile to local-state pref migrations (triggered by
// `MigrateObsoleteProfilePrefs()`).

TEST_F(BrowserPrefsTest, MigrateNTPLensBadgeCountFromProfileToLocal) {
  const int test_value = 3;

  profile_prefs()->SetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount,
                              test_value);

  ASSERT_EQ(
      profile_prefs()->GetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount),
      test_value);
  ASSERT_TRUE(local_state()
                  ->FindPreference(prefs::kNTPLensEntryPointNewBadgeShownCount)
                  ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(profile_prefs()
                  ->FindPreference(prefs::kNTPLensEntryPointNewBadgeShownCount)
                  ->IsDefaultValue());
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount),
      test_value);
}

TEST_F(BrowserPrefsTest,
       MigrateNTPHomeCustomizationBadgeCountFromProfileToLocal) {
  const int test_value = 99;

  profile_prefs()->SetInteger(
      prefs::kNTPHomeCustomizationNewBadgeImpressionCount, test_value);

  ASSERT_EQ(profile_prefs()->GetInteger(
                prefs::kNTPHomeCustomizationNewBadgeImpressionCount),
            test_value);
  ASSERT_TRUE(
      local_state()
          ->FindPreference(prefs::kNTPHomeCustomizationNewBadgeImpressionCount)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(
      profile_prefs()
          ->FindPreference(prefs::kNTPHomeCustomizationNewBadgeImpressionCount)
          ->IsDefaultValue());
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kNTPHomeCustomizationNewBadgeImpressionCount),
            test_value);
}

// [3] Profile pref renaming (triggered by `MigrateObsoleteProfilePrefs()`).

TEST_F(BrowserPrefsTest, RenameSafetyCheckModuleEnabledProfilePref) {
  const bool test_value = false;  // Default is true

  profile_prefs()->SetBoolean(
      prefs::kHomeCustomizationMagicStackSafetyCheckEnabled, test_value);

  ASSERT_EQ(profile_prefs()->GetBoolean(
                prefs::kHomeCustomizationMagicStackSafetyCheckEnabled),
            test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(safety_check::prefs::kSafetyCheckHomeModuleEnabled)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(profile_prefs()
                  ->FindPreference(
                      prefs::kHomeCustomizationMagicStackSafetyCheckEnabled)
                  ->IsDefaultValue());
  EXPECT_EQ(profile_prefs()->GetBoolean(
                safety_check::prefs::kSafetyCheckHomeModuleEnabled),
            test_value);
}

TEST_F(BrowserPrefsTest, RenameTabResumptionModuleEnabledProfilePref) {
  const bool test_value = false;  // Default is true

  profile_prefs()->SetBoolean(
      prefs::kHomeCustomizationMagicStackTabResumptionEnabled, test_value);

  ASSERT_EQ(profile_prefs()->GetBoolean(
                prefs::kHomeCustomizationMagicStackTabResumptionEnabled),
            test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(ntp_tiles::prefs::kTabResumptionHomeModuleEnabled)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(profile_prefs()
                  ->FindPreference(
                      prefs::kHomeCustomizationMagicStackTabResumptionEnabled)
                  ->IsDefaultValue());
  EXPECT_EQ(profile_prefs()->GetBoolean(
                ntp_tiles::prefs::kTabResumptionHomeModuleEnabled),
            test_value);
}

TEST_F(BrowserPrefsTest, RenameTipsModuleEnabledProfilePref) {
  const bool test_value = false;  // Default is true

  profile_prefs()->SetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled,
                              test_value);

  ASSERT_EQ(profile_prefs()->GetBoolean(
                prefs::kHomeCustomizationMagicStackTipsEnabled),
            test_value);
  ASSERT_TRUE(profile_prefs()
                  ->FindPreference(ntp_tiles::prefs::kTipsHomeModuleEnabled)
                  ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(
      profile_prefs()
          ->FindPreference(prefs::kHomeCustomizationMagicStackTipsEnabled)
          ->IsDefaultValue());
  EXPECT_EQ(
      profile_prefs()->GetBoolean(ntp_tiles::prefs::kTipsHomeModuleEnabled),
      test_value);
}

TEST_F(BrowserPrefsTest, RenameMagicStackEnabledProfilePref) {
  const bool test_value = false;  // Default is true

  profile_prefs()->SetBoolean(prefs::kHomeCustomizationMagicStackEnabled,
                              test_value);

  ASSERT_EQ(
      profile_prefs()->GetBoolean(prefs::kHomeCustomizationMagicStackEnabled),
      test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(ntp_tiles::prefs::kMagicStackHomeModuleEnabled)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(profile_prefs()
                  ->FindPreference(prefs::kHomeCustomizationMagicStackEnabled)
                  ->IsDefaultValue());
  EXPECT_EQ(profile_prefs()->GetBoolean(
                ntp_tiles::prefs::kMagicStackHomeModuleEnabled),
            test_value);
}

TEST_F(BrowserPrefsTest, RenamePriceTrackingModuleEnabledProfilePref) {
  const bool test_value = false;  // Default is true

  profile_prefs()->SetBoolean(
      prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled,
      test_value);

  ASSERT_EQ(
      profile_prefs()->GetBoolean(
          prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled),
      test_value);
  ASSERT_TRUE(profile_prefs()
                  ->FindPreference(commerce::kPriceTrackingHomeModuleEnabled)
                  ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(
      profile_prefs()
          ->FindPreference(
              prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled)
          ->IsDefaultValue());
  EXPECT_EQ(
      profile_prefs()->GetBoolean(commerce::kPriceTrackingHomeModuleEnabled),
      test_value);
}

TEST_F(BrowserPrefsTest, RenameMostVisitedModuleEnabledProfilePref) {
  const bool test_value = false;  // Default is true

  profile_prefs()->SetBoolean(prefs::kHomeCustomizationMostVisitedEnabled,
                              test_value);

  ASSERT_EQ(
      profile_prefs()->GetBoolean(prefs::kHomeCustomizationMostVisitedEnabled),
      test_value);
  ASSERT_TRUE(
      profile_prefs()
          ->FindPreference(ntp_tiles::prefs::kMostVisitedHomeModuleEnabled)
          ->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(profile_prefs()
                  ->FindPreference(prefs::kHomeCustomizationMostVisitedEnabled)
                  ->IsDefaultValue());
  EXPECT_EQ(profile_prefs()->GetBoolean(
                ntp_tiles::prefs::kMostVisitedHomeModuleEnabled),
            test_value);
}

// [4] `NSUserDefaults` migrations (triggered by
// `MigrateObsoleteProfilePrefs()`).

TEST_F(BrowserPrefsTest, MigrateSyncDisabledAlertShownFromUserDefaults) {
  NSString* kSyncDisabledAlertShownKey = @"SyncDisabledAlertShown";

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES forKey:kSyncDisabledAlertShownKey];

  ASSERT_TRUE([defaults boolForKey:kSyncDisabledAlertShownKey]);
  const PrefService::Preference* sync_disabled_alert_shown_pref =
      profile_prefs()->FindPreference(
          policy::policy_prefs::kSyncDisabledAlertShown);
  ASSERT_TRUE(sync_disabled_alert_shown_pref);
  ASSERT_TRUE(sync_disabled_alert_shown_pref->IsDefaultValue());

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(profile_prefs()->GetBoolean(
      policy::policy_prefs::kSyncDisabledAlertShown));
  EXPECT_FALSE(sync_disabled_alert_shown_pref->IsDefaultValue());
  EXPECT_EQ([defaults objectForKey:kSyncDisabledAlertShownKey], nil);

  MigrateObsoleteProfilePrefs(profile_prefs());

  EXPECT_TRUE(profile_prefs()->GetBoolean(
      policy::policy_prefs::kSyncDisabledAlertShown));
  EXPECT_EQ([defaults objectForKey:kSyncDisabledAlertShownKey], nil);
}

// [5] Local-state pref migrations and cleanup (triggered by
// `MigrateObsoleteLocalStatePrefs()`).

TEST_F(BrowserPrefsTest, RenameBottomOmniboxLocalStatePref) {
  const bool test_value = true;  // Default is false

  local_state()->SetBoolean(prefs::kBottomOmnibox, test_value);

  ASSERT_EQ(local_state()->GetBoolean(prefs::kBottomOmnibox), test_value);
  ASSERT_TRUE(local_state()
                  ->FindPreference(omnibox::kIsOmniboxInBottomPosition)
                  ->IsDefaultValue());

  MigrateObsoleteLocalStatePrefs(local_state());

  EXPECT_TRUE(
      local_state()->FindPreference(prefs::kBottomOmnibox)->IsDefaultValue());
  EXPECT_EQ(local_state()->GetBoolean(omnibox::kIsOmniboxInBottomPosition),
            test_value);
}

TEST_F(BrowserPrefsTest, CleanupObsoleteLocalStatePrefs) {
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness,
      4);
  local_state()->SetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount, 3);
  local_state()->SetInteger(prefs::kNTPHomeCustomizationNewBadgeImpressionCount,
                            99);

  ASSERT_FALSE(
      local_state()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness)
          ->IsDefaultValue());
  ASSERT_FALSE(local_state()
                   ->FindPreference(prefs::kNTPLensEntryPointNewBadgeShownCount)
                   ->IsDefaultValue());
  ASSERT_FALSE(
      local_state()
          ->FindPreference(prefs::kNTPHomeCustomizationNewBadgeImpressionCount)
          ->IsDefaultValue());

  MigrateObsoleteLocalStatePrefs(local_state());

  EXPECT_TRUE(
      local_state()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness)
          ->IsDefaultValue());
  EXPECT_TRUE(local_state()
                  ->FindPreference(prefs::kNTPLensEntryPointNewBadgeShownCount)
                  ->IsDefaultValue());
  EXPECT_TRUE(
      local_state()
          ->FindPreference(prefs::kNTPHomeCustomizationNewBadgeImpressionCount)
          ->IsDefaultValue());
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"

#import "components/commerce/core/pref_names.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/safety_check/safety_check_pref_names.h"
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

// Check that the migration of a pref from localState prefService to
// profile prefService is performed correctly.
TEST_F(BrowserPrefsTest, VerifyLocalStatePrefsMigration) {
  // Setup test data
  base::Value::List list_example = base::Value::List().Append("Example");
  base::Value::Dict dict_example;
  dict_example.Set("Example_key", "Example_value");

  // Set initial values in local_state

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
  local_state()->SetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount, 3);
  local_state()->SetInteger(prefs::kNTPHomeCustomizationNewBadgeImpressionCount,
                            99);

  // Set the old Safety Check module pref value to test its migration to the new
  // name.
  pref_service_.SetBoolean(
      prefs::kHomeCustomizationMagicStackSafetyCheckEnabled, false);

  // Set the old Tab Resumption module pref value to test its migration to the
  // new name.
  pref_service_.SetBoolean(
      prefs::kHomeCustomizationMagicStackTabResumptionEnabled, false);

  // Set the old Magic Stack Tips module pref value to test its migration to
  // the new name.
  pref_service_.SetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled,
                           false);
  // Set the old Magic Stack module pref value to test its migration to
  // the new name.
  pref_service_.SetBoolean(prefs::kHomeCustomizationMagicStackEnabled, false);

  // Set the old Price Tracking module pref value to test its migration to the
  // new name.
  pref_service_.SetBoolean(
      prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled, false);

  // Set the old Most Visited Tiles module pref value to test its migration to
  // the new name.
  pref_service_.SetBoolean(prefs::kHomeCustomizationMostVisitedEnabled, false);

  // Bottom omnibox position
  local_state()->SetBoolean(prefs::kBottomOmnibox, true);

  // Verify initial state before migration.

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
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount),
      3);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kNTPHomeCustomizationNewBadgeImpressionCount),
            99);

  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kHomeCustomizationMagicStackSafetyCheckEnabled));
  EXPECT_TRUE(
      pref_service_
          .FindPreference(safety_check::prefs::kSafetyCheckHomeModuleEnabled)
          ->IsDefaultValue());

  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kHomeCustomizationMagicStackTabResumptionEnabled));
  EXPECT_TRUE(
      pref_service_
          .FindPreference(ntp_tiles::prefs::kTabResumptionHomeModuleEnabled)
          ->IsDefaultValue());

  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled));
  EXPECT_TRUE(
      pref_service_.FindPreference(ntp_tiles::prefs::kTipsHomeModuleEnabled)
          ->IsDefaultValue());

  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kHomeCustomizationMagicStackEnabled));
  EXPECT_TRUE(
      pref_service_
          .FindPreference(ntp_tiles::prefs::kMagicStackHomeModuleEnabled)
          ->IsDefaultValue());

  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled));
  EXPECT_TRUE(
      pref_service_.FindPreference(commerce::kPriceTrackingHomeModuleEnabled)
          ->IsDefaultValue());

  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kHomeCustomizationMostVisitedEnabled));
  EXPECT_TRUE(
      pref_service_
          .FindPreference(ntp_tiles::prefs::kMostVisitedHomeModuleEnabled)
          ->IsDefaultValue());

  // Check bottom omnibox position.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kBottomOmnibox));
  EXPECT_TRUE(local_state()
                  ->FindPreference(omnibox::kIsOmniboxInBottomPosition)
                  ->IsDefaultValue());

  // Perform migration
  MigrateObsoleteLocalStatePrefs(local_state());
  MigrateObsoleteProfilePrefs(&pref_service_);

  // Verify state after migration.

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
  EXPECT_EQ(
      local_state()->GetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount),
      0);
  EXPECT_EQ(local_state()->GetInteger(
                prefs::kNTPHomeCustomizationNewBadgeImpressionCount),
            0);

  EXPECT_TRUE(
      pref_service_
          .FindPreference(prefs::kHomeCustomizationMagicStackSafetyCheckEnabled)
          ->IsDefaultValue());
  // The new pref `safety_check::prefs::kSafetyCheckHomeModuleEnabled` should
  // now be false (the migrated value).
  EXPECT_FALSE(pref_service_.GetBoolean(
      safety_check::prefs::kSafetyCheckHomeModuleEnabled));

  EXPECT_TRUE(pref_service_
                  .FindPreference(
                      prefs::kHomeCustomizationMagicStackTabResumptionEnabled)
                  ->IsDefaultValue());
  // The new pref `ntp_tiles::prefs::kTabResumptionHomeModuleEnabled` should
  // now be false (the migrated value).
  EXPECT_FALSE(pref_service_.GetBoolean(
      ntp_tiles::prefs::kTabResumptionHomeModuleEnabled));

  EXPECT_TRUE(
      pref_service_
          .FindPreference(prefs::kHomeCustomizationMagicStackTipsEnabled)
          ->IsDefaultValue());
  // The new pref `ntp_tiles::prefs::kTipsHomeModuleEnabled` should
  // now be false (the migrated value).
  EXPECT_FALSE(
      pref_service_.GetBoolean(ntp_tiles::prefs::kTipsHomeModuleEnabled));

  EXPECT_TRUE(
      pref_service_.FindPreference(prefs::kHomeCustomizationMagicStackEnabled)
          ->IsDefaultValue());
  // The new pref `ntp_tiles::prefs::kMagicStackHomeModuleEnabled` should
  // now be false (the migrated value).
  EXPECT_FALSE(
      pref_service_.GetBoolean(ntp_tiles::prefs::kMagicStackHomeModuleEnabled));

  EXPECT_TRUE(
      pref_service_
          .FindPreference(
              prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled)
          ->IsDefaultValue());
  // The new pref `commerce::kPriceTrackingHomeModuleEnabled` should
  // now be false (the migrated value).
  EXPECT_FALSE(
      pref_service_.GetBoolean(commerce::kPriceTrackingHomeModuleEnabled));

  EXPECT_TRUE(
      pref_service_.FindPreference(prefs::kHomeCustomizationMostVisitedEnabled)
          ->IsDefaultValue());
  // The new pref `ntp_tiles::prefs::kMostVisitedHomeModuleEnabled` should
  // now be false (the migrated value).
  EXPECT_FALSE(pref_service_.GetBoolean(
      ntp_tiles::prefs::kMostVisitedHomeModuleEnabled));

  // Check bottom omnibox position.
  EXPECT_TRUE(
      local_state()->FindPreference(prefs::kBottomOmnibox)->IsDefaultValue());
  EXPECT_TRUE(local_state()->GetBoolean(omnibox::kIsOmniboxInBottomPosition));
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

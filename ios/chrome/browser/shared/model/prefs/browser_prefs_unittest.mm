// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"

#import <string_view>

#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/safety_check/safety_check_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
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

    // TODO(crbug.com/369296278): Remove this line ~one year after full launch.
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

// [1] Profile pref renaming (triggered by `MigrateObsoleteProfilePrefs()`).

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

// [2] Local-state pref migrations and cleanup (triggered by
// `MigrateObsoleteLocalStatePrefs()`).

TEST_F(BrowserPrefsTest, CleanupObsoleteLocalStatePrefs) {
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness,
      4);

  ASSERT_FALSE(
      local_state()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness)
          ->IsDefaultValue());

  MigrateObsoleteLocalStatePrefs(local_state());

  EXPECT_TRUE(
      local_state()
          ->FindPreference(
              prefs::
                  kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness)
          ->IsDefaultValue());
}

TEST_F(BrowserPrefsTest, RegisterPersonalContextPrefs) {
  EXPECT_NE(profile_prefs()->FindPreference(
                personal_context::prefs::
                    kPersonalContextAmbientAutofillNoticeShouldBeShown),
            nullptr);
  EXPECT_NE(profile_prefs()->FindPreference(
                personal_context::prefs::
                    kPersonalContextInAutofillSettingsToggleStatus),
            nullptr);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intents/model/user_activity_compatibility_util.h"

#import <Foundation/Foundation.h>

#import "base/memory/ptr_util.h"
#import "base/test/task_environment.h"
#import "base/values.h"
#import "components/handoff/handoff_utility.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class UserActivityCompatibilityUtilTest : public PlatformTest {
 public:
  UserActivityCompatibilityUtilTest() {
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  // Set pref kIncognitoModeAvailability to kForced and make it a managed pref.
  void ForceIncognitoMode() {
    PrefService* pref_service = profile_->GetPrefs();
    profile_->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kIncognitoModeAvailability,
        base::Value(static_cast<int>(IncognitoModePrefs::kForced)));

    EXPECT_TRUE(pref_service->IsManagedPreference(
        policy::policy_prefs::kIncognitoModeAvailability));

    EXPECT_TRUE(IsIncognitoModeForced(pref_service));
  }

  // Set pref kIncognitoModeAvailability to kDisabled and make it a managed
  // pref.
  void DisableIncognitoMode() {
    PrefService* pref_service = profile_->GetPrefs();
    profile_->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kIncognitoModeAvailability,
        base::Value(static_cast<int>(IncognitoModePrefs::kDisabled)));

    EXPECT_TRUE(pref_service->IsManagedPreference(
        policy::policy_prefs::kIncognitoModeAvailability));

    EXPECT_TRUE(IsIncognitoModeDisabled(pref_service));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that method ProceedWithUserActivity returns true when incognito mode
// is forced and when userActivity supports incognito browser.
TEST_F(UserActivityCompatibilityUtilTest,
       ProceedWithUserActivityWithIncognitoBrowser) {
  // UserActivityTypes to test.
  NSArray* user_activity_types = @[
    handoff::kChromeHandoffActivityType, kSiriShortcutOpenInIncognito,
    kSiriOpenLatestTab, kSiriOpenReadingList, kSiriOpenBookmarks,
    kSiriOpenTabGrid, kSiriVoiceSearch, kSiriOpenNewTab, kSiriPlayDinoGame,
    kSiriSetChromeDefaultBrowser, kSiriManagePaymentMethods,
    kSiriRunSafetyCheck, kSiriManagePasswords, kSiriManageSettings,
    kSiriOpenLensFromIntents, kSiriOpenNewIncognitoTab
  ];

  ForceIncognitoMode();

  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];

    EXPECT_TRUE(ProceedWithUserActivity(user_activity, profile_->GetPrefs()));
  }
}

// Tests that method canProceedWithUserActivity returns false when incognito
// mode is forced and when userActivity does not support incognito browser.
TEST_F(UserActivityCompatibilityUtilTest,
       ProceedWithWrongUserActivityWithIncognitoBrowser) {
  ForceIncognitoMode();

  NSArray* user_activity_types = @[
    kSiriShortcutOpenInChrome, kSiriOpenRecentTabs, kSiriViewHistory,
    kSiriClearBrowsingData
  ];

  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];
    EXPECT_FALSE(ProceedWithUserActivity(user_activity, profile_->GetPrefs()));
  }
}

// Tests that method canProceedWithUserActivity returns true when incognito mode
// is disabled and when userActivity supports regular browser.
TEST_F(UserActivityCompatibilityUtilTest,
       CanProceedWithUserActivityWithRegularBrowser) {
  // UserActivityTypes to test.
  NSArray* user_activity_types = @[
    handoff::kChromeHandoffActivityType, kSiriShortcutSearchInChrome,
    kSiriShortcutOpenInChrome, kSiriOpenLatestTab, kSiriOpenReadingList,
    kSiriOpenBookmarks, kSiriOpenTabGrid, kSiriVoiceSearch, kSiriOpenNewTab,
    kSiriPlayDinoGame, kSiriSetChromeDefaultBrowser, kSiriManagePaymentMethods,
    kSiriRunSafetyCheck, kSiriManagePasswords, kSiriManageSettings,
    kSiriOpenLensFromIntents, kSiriOpenRecentTabs, kSiriViewHistory,
    kSiriClearBrowsingData
  ];

  DisableIncognitoMode();

  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];

    EXPECT_TRUE(ProceedWithUserActivity(user_activity, profile_->GetPrefs()));
  }
}

// Tests that method canProceedWithUserActivity returns false when incognito
// mode is disabled and when userActivity does not support regular browser.
TEST_F(UserActivityCompatibilityUtilTest,
       CanProceedWithWrongUserActivityWithRegularBrowser) {
  // UserActivityTypes to test.
  NSArray* user_activity_types =
      @[ kSiriShortcutOpenInIncognito, kSiriOpenNewIncognitoTab ];

  DisableIncognitoMode();

  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];

    EXPECT_FALSE(ProceedWithUserActivity(user_activity, profile_->GetPrefs()));
  }
}

// Tests that method canProceedWithUserActivity returns false if the activity
// type is unknown.
TEST_F(UserActivityCompatibilityUtilTest,
       CanProceedWithUserActivityWithWrongActivityType) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:@"not_an_activity_type"];
  EXPECT_FALSE(ProceedWithUserActivity(user_activity, profile_->GetPrefs()));
}

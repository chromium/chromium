// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_preferences_internal.h"

#import <Foundation/Foundation.h>
#import <memory>

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/pref_service_factory.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

class CWVPreferencesTest : public PlatformTest {
 protected:
  // Creates a new pref service and optionally deletes the backing file for a
  // clean slate.
  std::unique_ptr<PrefService> CreateTestPrefService(bool delete_file = true) {
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
        new user_prefs::PrefRegistrySyncable;
    pref_registry->RegisterBooleanPref(
        autofill::prefs::kAutofillCreditCardEnabled, true);
    pref_registry->RegisterBooleanPref(autofill::prefs::kAutofillProfileEnabled,
                                       true);
    pref_registry->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    pref_registry->RegisterBooleanPref(translate::prefs::kOfferTranslateEnabled,
                                       true);
    pref_registry->RegisterBooleanPref(
        password_manager::prefs::kPasswordLeakDetectionEnabled, true);

    pref_registry->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
    pref_registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);

    base::FilePath temp_dir_path;
    EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir_path));

    base::FilePath temp_prefs_path = temp_dir_path.Append("TestPrefs");
    if (delete_file) {
      EXPECT_TRUE(base::DeleteFile(temp_prefs_path));
    }

    scoped_refptr<PersistentPrefStore> pref_store =
        new JsonPrefStore(temp_prefs_path);
    PrefServiceFactory factory;
    factory.set_user_prefs(pref_store);

    return factory.Create(pref_registry.get());
  }

  web::WebTaskEnvironment task_environment_;
};

// Tests CWVPreferences |profileAutofillEnabled|.
TEST_F(CWVPreferencesTest, ProfileAutofillEnabled) {
  std::unique_ptr<PrefService> pref_service = CreateTestPrefService();
  CWVPreferences* preferences =
      [[CWVPreferences alloc] initWithPrefService:pref_service.get()];
  EXPECT_TRUE(preferences.profileAutofillEnabled);
  preferences.profileAutofillEnabled = NO;
  EXPECT_FALSE(preferences.profileAutofillEnabled);
}

// Tests CWVPreferences |creditCardAutofillEnabled|.
TEST_F(CWVPreferencesTest, CreditCardAutofillEnabled) {
  std::unique_ptr<PrefService> pref_service = CreateTestPrefService();
  CWVPreferences* preferences =
      [[CWVPreferences alloc] initWithPrefService:pref_service.get()];
  EXPECT_TRUE(preferences.creditCardAutofillEnabled);
  preferences.creditCardAutofillEnabled = NO;
  EXPECT_FALSE(preferences.creditCardAutofillEnabled);
}

// Tests CWVPreferences |translationEnabled|.
TEST_F(CWVPreferencesTest, TranslationEnabled) {
  std::unique_ptr<PrefService> pref_service = CreateTestPrefService();
  CWVPreferences* preferences =
      [[CWVPreferences alloc] initWithPrefService:pref_service.get()];
  EXPECT_TRUE(preferences.translationEnabled);
  preferences.translationEnabled = NO;
  EXPECT_FALSE(preferences.translationEnabled);
}

// Tests CWVPreferences |passwordAutofillEnabled|.
TEST_F(CWVPreferencesTest, PasswordAutofillEnabled) {
  std::unique_ptr<PrefService> pref_service = CreateTestPrefService();
  CWVPreferences* preferences =
      [[CWVPreferences alloc] initWithPrefService:pref_service.get()];
  EXPECT_TRUE(preferences.passwordAutofillEnabled);
  preferences.passwordAutofillEnabled = NO;
  EXPECT_FALSE(preferences.passwordAutofillEnabled);
}

// Tests CWVPreferences |passwordLeakCheckEnabled|.
TEST_F(CWVPreferencesTest, PasswordLeakCheckEnabled) {
  std::unique_ptr<PrefService> pref_service = CreateTestPrefService();
  CWVPreferences* preferences =
      [[CWVPreferences alloc] initWithPrefService:pref_service.get()];
  EXPECT_TRUE(preferences.passwordLeakCheckEnabled);
  preferences.passwordLeakCheckEnabled = NO;
  EXPECT_FALSE(preferences.passwordLeakCheckEnabled);
}

// Tests safe browsing setting.
TEST_F(CWVPreferencesTest, SafeBrowsingEnabled) {
  std::unique_ptr<PrefService> pref_service = CreateTestPrefService();
  CWVPreferences* preferences =
      [[CWVPreferences alloc] initWithPrefService:pref_service.get()];
  EXPECT_TRUE(preferences.safeBrowsingEnabled);
  preferences.safeBrowsingEnabled = NO;
  EXPECT_FALSE(preferences.safeBrowsingEnabled);
}

// Tests pending writes are committed to disk.
TEST_F(CWVPreferencesTest, CommitPendingWrite) {
  std::unique_ptr<PrefService> pref_service = CreateTestPrefService();
  CWVPreferences* preferences =
      [[CWVPreferences alloc] initWithPrefService:pref_service.get()];

  EXPECT_TRUE(preferences.safeBrowsingEnabled);
  preferences.safeBrowsingEnabled = NO;
  EXPECT_FALSE(preferences.safeBrowsingEnabled);

  __block BOOL commit_completion_was_called = NO;
  [preferences commitPendingWrite:^{
    commit_completion_was_called = YES;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return commit_completion_was_called;
  }));

  pref_service = CreateTestPrefService(/*delete_file=*/false);
  preferences = [[CWVPreferences alloc] initWithPrefService:pref_service.get()];
  EXPECT_FALSE(preferences.safeBrowsingEnabled);
}

}  // namespace ios_web_view

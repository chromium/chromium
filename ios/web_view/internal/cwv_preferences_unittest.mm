// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_preferences_internal.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVPreferencesTest : public PlatformTest {
 protected:
  CWVPreferencesTest() {
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

    scoped_refptr<PersistentPrefStore> pref_store = new InMemoryPrefStore();
    PrefServiceFactory factory;
    factory.set_user_prefs(pref_store);

    pref_service_ = factory.Create(pref_registry.get());
    preferences_ =
        [[CWVPreferences alloc] initWithPrefService:pref_service_.get()];
  }

  std::unique_ptr<PrefService> pref_service_;
  CWVPreferences* preferences_;
};

// Tests CWVPreferences |profileAutofillEnabled|.
TEST_F(CWVPreferencesTest, ProfileAutofillEnabled) {
  EXPECT_TRUE(preferences_.profileAutofillEnabled);
  preferences_.profileAutofillEnabled = NO;
  EXPECT_FALSE(preferences_.profileAutofillEnabled);
}

// Tests CWVPreferences |creditCardAutofillEnabled|.
TEST_F(CWVPreferencesTest, CreditCardAutofillEnabled) {
  EXPECT_TRUE(preferences_.creditCardAutofillEnabled);
  preferences_.creditCardAutofillEnabled = NO;
  EXPECT_FALSE(preferences_.creditCardAutofillEnabled);
}

// Tests CWVPreferences |translationEnabled|.
TEST_F(CWVPreferencesTest, TranslationEnabled) {
  EXPECT_TRUE(preferences_.translationEnabled);
  preferences_.translationEnabled = NO;
  EXPECT_FALSE(preferences_.translationEnabled);
}

// Tests CWVPreferences |passwordAutofillEnabled|.
TEST_F(CWVPreferencesTest, PasswordAutofillEnabled) {
  EXPECT_TRUE(preferences_.passwordAutofillEnabled);
  preferences_.passwordAutofillEnabled = NO;
  EXPECT_FALSE(preferences_.passwordAutofillEnabled);
}

// Tests CWVPreferences |passwordLeakCheckEnabled|.
TEST_F(CWVPreferencesTest, PasswordLeakCheckEnabled) {
  EXPECT_TRUE(preferences_.passwordLeakCheckEnabled);
  preferences_.passwordLeakCheckEnabled = NO;
  EXPECT_FALSE(preferences_.passwordLeakCheckEnabled);
}

// Tests safe browsing setting.
TEST_F(CWVPreferencesTest, SafeBrowsingEnabled) {
  EXPECT_TRUE(preferences_.safeBrowsingEnabled);
  preferences_.safeBrowsingEnabled = NO;
  EXPECT_FALSE(preferences_.safeBrowsingEnabled);
}

}  // namespace ios_web_view

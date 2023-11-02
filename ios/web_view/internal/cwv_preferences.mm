// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_preferences_internal.h"

#include "components/autofill/core/common/autofill_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVPreferences {
  PrefService* _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - Public Methods

- (void)setTranslationEnabled:(BOOL)enabled {
  _prefService->SetBoolean(translate::prefs::kOfferTranslateEnabled, enabled);
}

- (BOOL)isTranslationEnabled {
  return _prefService->GetBoolean(translate::prefs::kOfferTranslateEnabled);
}

- (void)resetTranslationSettings {
  translate::TranslatePrefs translatePrefs(_prefService);
  translatePrefs.ResetToDefaults();
}

- (void)setProfileAutofillEnabled:(BOOL)enabled {
  autofill::prefs::SetAutofillProfileEnabled(_prefService, enabled);
}

- (BOOL)isProfileAutofillEnabled {
  return autofill::prefs::IsAutofillProfileEnabled(_prefService);
}

- (void)setCreditCardAutofillEnabled:(BOOL)enabled {
  autofill::prefs::SetAutofillCreditCardEnabled(_prefService, enabled);
}

- (BOOL)isCreditCardAutofillEnabled {
  return autofill::prefs::IsAutofillCreditCardEnabled(_prefService);
}

- (void)setPasswordAutofillEnabled:(BOOL)enabled {
  _prefService->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                           enabled);
}

- (BOOL)isPasswordAutofillEnabled {
  return _prefService->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
}

- (void)setPasswordLeakCheckEnabled:(BOOL)enabled {
  _prefService->SetBoolean(
      password_manager::prefs::kPasswordLeakDetectionEnabled, enabled);
}

- (BOOL)isPasswordLeakCheckEnabled {
  return _prefService->GetBoolean(
      password_manager::prefs::kPasswordLeakDetectionEnabled);
}

- (void)setSafeBrowsingEnabled:(BOOL)enabled {
  safe_browsing::SetSafeBrowsingState(
      _prefService,
      enabled ? safe_browsing::SafeBrowsingState::STANDARD_PROTECTION
              : safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING,
      /*is_esb_enabled_in_sync=*/false);
}

- (BOOL)isSafeBrowsingEnabled {
  return safe_browsing::IsSafeBrowsingEnabled(*_prefService);
}

@end

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_preferences_internal.h"

#import "base/functional/bind.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/language/core/browser/pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"

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
  autofill::prefs::SetAutofillPaymentMethodsEnabled(_prefService, enabled);
}

- (BOOL)isCreditCardAutofillEnabled {
  return autofill::prefs::IsAutofillPaymentMethodsEnabled(_prefService);
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

- (void)commitPendingWrite:(void (^)(void))completionHandler {
  _prefService->CommitPendingWrite(base::BindOnce(^{
    if (completionHandler) {
      completionHandler();
    }
  }));
}

@end

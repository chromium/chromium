// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_consumer.h"

@interface AutofillAndPasswordsMediator () <PrefObserverDelegate>
@end

@implementation AutofillAndPasswordsMediator {
  raw_ptr<PrefService> _userPrefService;
  raw_ptr<autofill::EntityDataManager> _entityDataManager;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                      entityDataManager:
                          (autofill::EntityDataManager*)entityDataManager {
  self = [super init];
  if (self) {
    _userPrefService = userPrefService;
    _entityDataManager = entityDataManager;
    _prefChangeRegistrar.Init(_userPrefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    _prefObserverBridge->ObserveChangesForPreference(
        password_manager::prefs::kCredentialsEnableService,
        &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillCreditCardEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillProfileEnabled, &_prefChangeRegistrar);
  }
  return self;
}

- (void)setConsumer:(id<AutofillAndPasswordsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  if (_consumer && _userPrefService) {
    [_consumer setPasswordsEnabled:
                   _userPrefService->GetBoolean(
                       password_manager::prefs::kCredentialsEnableService)];

    [_consumer setAutofillCreditCardEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillCreditCardEnabled)];

    [_consumer setAutofillProfileEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillProfileEnabled)];

    // TODO(crbug.com/491417038): Introduce logic to enable/disable values based
    // on pref value.
    [_consumer setIdentityDocsEnabled:YES];
    [_consumer setTravelInfoEnabled:YES];

    [_consumer setShouldShowAutofillAIFeatures:_entityDataManager != nullptr];
  }
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _userPrefService = nullptr;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == password_manager::prefs::kCredentialsEnableService) {
    [_consumer setPasswordsEnabled:
                   _userPrefService->GetBoolean(
                       password_manager::prefs::kCredentialsEnableService)];
  }

  if (preferenceName == autofill::prefs::kAutofillProfileEnabled) {
    [_consumer setAutofillProfileEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillProfileEnabled)];
  }

  if (preferenceName == autofill::prefs::kAutofillCreditCardEnabled) {
    [_consumer setAutofillCreditCardEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillCreditCardEnabled)];
  }
}

@end

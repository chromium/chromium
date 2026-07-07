// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_consumer.h"

@interface AutofillAndPasswordsMediator () <
    IOSAutofillEntityDataManagerObserver,
    PrefObserverDelegate>
@end

@implementation AutofillAndPasswordsMediator {
  raw_ptr<PrefService> _userPrefService;
  raw_ptr<autofill::EntityDataManager> _entityDataManager;
  std::unique_ptr<autofill::IOSAutofillEntityDataManagerObserverBridge>
      _entityDataManagerObserver;
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
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillAiIdentityEntitiesEnabled,
        &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillAiTravelEntitiesEnabled,
        &_prefChangeRegistrar);

    if (_entityDataManager) {
      _entityDataManagerObserver = std::make_unique<
          autofill::IOSAutofillEntityDataManagerObserverBridge>(
          _entityDataManager, self);
    }
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

    [_consumer setIdentityDocsEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillAiIdentityEntitiesEnabled)];
    [_consumer setTravelInfoEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillAiTravelEntitiesEnabled)];
    // TODO(crbug.com/530620605): Introduce logic to enable/disable values based
    // on pref value.
    if (autofill::IsAmbientAutofillEnabled()) {
      [_consumer setShoppingEnabled:YES];
    }
    [self updateShouldShowAutofillAIFeatures];
  }
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _entityDataManagerObserver.reset();
  _entityDataManager = nullptr;
  _userPrefService = nullptr;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == password_manager::prefs::kCredentialsEnableService) {
    [_consumer setPasswordsEnabled:
                   _userPrefService->GetBoolean(
                       password_manager::prefs::kCredentialsEnableService)];
  } else if (preferenceName == autofill::prefs::kAutofillProfileEnabled) {
    [_consumer setAutofillProfileEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillProfileEnabled)];
  } else if (preferenceName == autofill::prefs::kAutofillCreditCardEnabled) {
    [_consumer setAutofillCreditCardEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillCreditCardEnabled)];
  } else if (preferenceName ==
             autofill::prefs::kAutofillAiIdentityEntitiesEnabled) {
    [_consumer setIdentityDocsEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillAiIdentityEntitiesEnabled)];
  } else if (preferenceName ==
             autofill::prefs::kAutofillAiTravelEntitiesEnabled) {
    [_consumer setTravelInfoEnabled:
                   _userPrefService->GetBoolean(
                       autofill::prefs::kAutofillAiTravelEntitiesEnabled)];
  }
}

#pragma mark - IOSAutofillEntityDataManagerObserver

- (void)onEntityInstancesChanged {
  [self updateShouldShowAutofillAIFeatures];
}

#pragma mark - Private

// Updates the consumer on whether to show Autofill AI features based on their
// availability and the presence of local data.
- (void)updateShouldShowAutofillAIFeatures {
  if (!_consumer) {
    return;
  }
  if (!_entityDataManager) {
    [_consumer setShouldShowAutofillAIFeatures:NO];
    return;
  }

  BOOL showFeatures = base::FeatureList::IsEnabled(
                          autofill::features::kAutofillAiWithDataSchema) ||
                      !_entityDataManager->GetEntityInstances().empty();
  [_consumer setShouldShowAutofillAIFeatures:showFeatures];
}

@end

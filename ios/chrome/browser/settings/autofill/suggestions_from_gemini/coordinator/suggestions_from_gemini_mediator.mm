// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "components/optimization_guide/core/feature_registry/feature_registration.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_consumer.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/utils/suggestions_from_gemini_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

@interface SuggestionsFromGeminiMediator () <BooleanObserver,
                                             PrefObserverDelegate>
@end

@implementation SuggestionsFromGeminiMediator {
  // The observable boolean backing the preference status of the Suggestions
  // from Gemini toggle.
  PrefBackedBoolean* _personalContextSwitchEnabled;
  raw_ptr<PrefService> _prefService;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(prefService);
    _prefService = prefService;
    _personalContextSwitchEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:personal_context::prefs::
                                kPersonalContextInAutofillSettingsToggleStatus];
    _personalContextSwitchEnabled.observer = self;

    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        optimization_guide::prefs::kFindAndFillWithGeminiSettings,
        &_prefChangeRegistrar);
  }
  return self;
}

- (void)setConsumer:(id<SuggestionsFromGeminiConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [_consumer
        setSuggestionsFromGeminiSwitchOn:_personalContextSwitchEnabled.value];
    [_consumer
        setSuggestionsFromGeminiPolicyState:GetSuggestionsFromGeminiPolicyState(
                                                _prefService)];
  }
}

- (void)disconnect {
  _personalContextSwitchEnabled.observer = nil;
  _personalContextSwitchEnabled = nil;
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nullptr;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName ==
      optimization_guide::prefs::kFindAndFillWithGeminiSettings) {
    if (self.consumer) {
      [self.consumer setSuggestionsFromGeminiPolicyState:
                         GetSuggestionsFromGeminiPolicyState(_prefService)];
    }
  }
}

#pragma mark - SuggestionsFromGeminiMutator

- (void)didToggleSuggestionsFromGeminiSwitch:(BOOL)on {
  _personalContextSwitchEnabled.value = on;
}

- (void)didSelectManageConnectedApps {
  [self.delegate suggestionsFromGeminiMediatorDidSelectConnectedApps:self];
}

- (void)didSelectHelpImprove {
  [self.delegate suggestionsFromGeminiMediatorDidSelectHelpImprove:self];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(PrefBackedBoolean*)boolean {
  CHECK_EQ(boolean, _personalContextSwitchEnabled);
  [self.consumer
      setSuggestionsFromGeminiSwitchOn:_personalContextSwitchEnabled.value];
}

@end

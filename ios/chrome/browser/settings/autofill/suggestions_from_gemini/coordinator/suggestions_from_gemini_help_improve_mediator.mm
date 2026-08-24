// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_help_improve_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/optimization_guide/core/feature_registry/feature_registration.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_consumer.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/utils/suggestions_from_gemini_utils.h"

@interface SuggestionsFromGeminiHelpImproveMediator () <PrefObserverDelegate>
@end

@implementation SuggestionsFromGeminiHelpImproveMediator {
  raw_ptr<PrefService> _prefService;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(prefService);
    _prefService = prefService;
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        optimization_guide::prefs::kFindAndFillWithGeminiSettings,
        &_prefChangeRegistrar);
  }
  return self;
}

- (void)setConsumer:(id<SuggestionsFromGeminiHelpImproveConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [_consumer
        setSuggestionsFromGeminiPolicyState:GetSuggestionsFromGeminiPolicyState(
                                                _prefService)];
  }
}

- (void)disconnect {
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

@end

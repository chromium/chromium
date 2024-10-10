// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_magic_stack_mediator.h"

#import <memory>

#import "base/check.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_prefs.h"

using segmentation_platform::TipIdentifier;

@interface TipsMagicStackMediator () <PrefObserverDelegate, TipsModuleAudience>
@end

@implementation TipsMagicStackMediator {
  // The profile Pref service.
  raw_ptr<PrefService> _profilePrefService;

  // Registrar for user Pref changes notifications.
  PrefChangeRegistrar _profilePrefChangeRegistrar;

  // Bridge to listen to Pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
}

- (instancetype)initWithIdentifier:(TipIdentifier)identifier
                profilePrefService:(PrefService*)profilePrefService {
  self = [super init];

  if (self) {
    CHECK(profilePrefService);

    _profilePrefService = profilePrefService;
    _state = [[TipsModuleState alloc] initWithIdentifier:identifier];
    _state.audience = self;

    if (!_prefObserverBridge) {
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

      _profilePrefChangeRegistrar.Init(profilePrefService);

      _prefObserverBridge->ObserveChangesForPreference(
          (IsHomeCustomizationEnabled()
               ? prefs::kHomeCustomizationMagicStackTipsEnabled
               : tips_prefs::kTipsInMagicStackDisabledPref),
          &_profilePrefChangeRegistrar);
    }
  }

  return self;
}

- (void)disconnect {
  if (_prefObserverBridge) {
    _profilePrefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
}

- (void)reconfigureWithTipIdentifier:(TipIdentifier)identifier {
  _state = [[TipsModuleState alloc] initWithIdentifier:identifier];
  _state.audience = self;
}

- (void)disableModule {
  tips_prefs::DisableTipsInMagicStack(_profilePrefService);
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (tips_prefs::IsTipsInMagicStackDisabled(_profilePrefService)) {
    [self.delegate removeTipsModule];
  }
}

#pragma mark - TipsModuleAudience

- (void)didSelectTip:(TipIdentifier)tip {
  [self.presentationAudience didSelectTip:tip];
}

@end

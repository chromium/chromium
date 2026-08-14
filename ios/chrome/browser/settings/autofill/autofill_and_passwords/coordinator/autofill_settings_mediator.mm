// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_consumer.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AutofillSettingsMediator () <PrefObserverDelegate> {
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Pref observer to track changes to prefs.
  std::optional<PrefObserverBridge> _prefObserverBridge;
}

@end

@implementation AutofillSettingsMediator {
  raw_ptr<PrefService> _prefs;
  raw_ptr<signin::IdentityManager> _identityManager;
  id<ReauthenticationProtocol> _reauthenticationModule;
  BOOL _shouldShowWalletPromo;
}

- (instancetype)initWithPrefService:(PrefService*)prefs
                    identityManager:(signin::IdentityManager*)identityManager
             reauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
              shouldShowWalletPromo:(BOOL)shouldShowWalletPromo {
  self = [super init];
  if (self) {
    _prefs = prefs;
    _identityManager = identityManager;
    _reauthenticationModule = reauthModule;
    _shouldShowWalletPromo = shouldShowWalletPromo;

    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.emplace(self);
    // Register to observe any changes on Pref-backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::GetAutofillAiOptInPreferenceKeyName(), &_prefChangeRegistrar);
  }
  return self;
}

- (void)setConsumer:(id<AutofillSettingsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  if (_consumer && _prefs && _identityManager) {
    BOOL enhancedAutofillEnabled = [self isEnhancedAutofillEnabled];
    BOOL canAttemptReauth = [_reauthenticationModule canAttemptReauth];
    [_consumer setAutofillAIAllowedByPolicy:
                   autofill::IsAutofillAiAllowedByEnterprisePolicy(_prefs)];
    [_consumer setEnhancedAutofillEnabled:enhancedAutofillEnabled];
    [_consumer setUserVerificationSettingVisible:
                   base::FeatureList::IsEnabled(
                       autofill::features::kAutofillAiReauthRequired)];
    [_consumer
        setUserVerificationEnabled:
            autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(_prefs)];
    [_consumer setUserVerificationSwitchEnabled:canAttemptReauth];
    [_consumer setShouldShowWalletPromo:_shouldShowWalletPromo];
  }
}

- (void)disconnect {
  // Remove pref changes registrations.
  _prefChangeRegistrar.RemoveAll();
  // Remove observer bridges.
  _prefObserverBridge.reset();

  _prefs = nullptr;
  _identityManager = nullptr;
  _reauthenticationModule = nil;
}

#pragma mark - AutofillSettingsMutator

- (void)setEnhancedAutofillEnabled:(BOOL)enabled {
  [self.delegate autofillSettingsMediator:self
                didToggleEnhancedAutofill:enabled];
  if (_consumer) {
    [_consumer setEnhancedAutofillEnabled:enabled];
  }
}

- (void)setUserVerificationEnabled:(BOOL)enabled {
  if (!_prefs) {
    return;
  }
  if (![_reauthenticationModule canAttemptReauth]) {
    [_consumer
        setUserVerificationEnabled:
            autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(_prefs)];
    return;
  }

  NSString* reauthReason = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_AUTOFILL_VERIFICATION_TOGGLE_REAUTH_REASON);

  __weak __typeof(self) weakSelf = self;
  [_reauthenticationModule
      attemptReauthWithLocalizedReason:reauthReason
                  canReusePreviousAuth:YES
                               handler:^(ReauthenticationResult result) {
                                 [weakSelf
                                     onReauthCompletedWithTargetState:enabled
                                                               result:result];
                               }];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == autofill::GetAutofillAiOptInPreferenceKeyName()) {
    [_consumer setEnhancedAutofillEnabled:[self isEnhancedAutofillEnabled]];
  }
}

#pragma mark - Private

- (BOOL)isEnhancedAutofillEnabled {
  return autofill::GetAutofillAiOptInStatus(_prefs, _identityManager);
}

- (void)onReauthCompletedWithTargetState:(BOOL)targetOn
                                  result:(ReauthenticationResult)result {
  if (!_prefs) {
    return;
  }
  if (result == ReauthenticationResult::kSuccess ||
      result == ReauthenticationResult::kSkipped) {
    autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(_prefs, targetOn);
  }
  [_consumer
      setUserVerificationEnabled:
          autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(_prefs)];
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_consumer.h"

@interface IncognitoLockMediator () <BooleanObserver>
@end

@implementation IncognitoLockMediator {
  raw_ptr<PrefService> _localState;
  PrefBackedBoolean* _incognitoReauthPref;
  PrefBackedBoolean* _incognitoSoftLockPref;
}

- (instancetype)initWithLocalState:(PrefService*)localState {
  self = [super init];
  if (self) {
    CHECK(localState);
    _localState = localState;

    _incognitoReauthPref = [[PrefBackedBoolean alloc]
        initWithPrefService:_localState
                   prefName:prefs::kIncognitoAuthenticationSetting];
    [_incognitoReauthPref setObserver:self];

    _incognitoSoftLockPref = [[PrefBackedBoolean alloc]
        initWithPrefService:_localState
                   prefName:prefs::kIncognitoSoftLockSetting];
    [_incognitoSoftLockPref setObserver:self];
  }
  return self;
}

- (void)disconnect {
  // Stop observable prefs.
  [_incognitoReauthPref stop];
  _incognitoReauthPref.observer = nil;
  _incognitoReauthPref = nil;

  [_incognitoSoftLockPref stop];
  _incognitoSoftLockPref.observer = nil;
  _incognitoSoftLockPref = nil;

  _localState = nullptr;
}

- (void)setConsumer:(id<IncognitoLockConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [self updateConsumer];
}

#pragma mark - IncognitoLockMutator

- (void)updateIncognitoLockState:(IncognitoLockState)state {
  switch (state) {
    case IncognitoLockState::kReauth:
      _incognitoReauthPref.value = true;
      _incognitoSoftLockPref.value = false;
      base::UmaHistogramEnumeration(
          kIncognitoLockSettingInteractionHistogram,
          IncognitoLockSettingInteraction::kHideWithReauthSelected);
      base::RecordAction(
          base::UserMetricsAction("IOS.Settings.IncognitoLock.HideWithReauth"));
      break;
    case IncognitoLockState::kSoftLock:
      _incognitoReauthPref.value = false;
      _incognitoSoftLockPref.value = true;
      base::UmaHistogramEnumeration(
          kIncognitoLockSettingInteractionHistogram,
          IncognitoLockSettingInteraction::kHideWithSoftLockSelected);
      base::RecordAction(base::UserMetricsAction(
          "IOS.Settings.IncognitoLock.HideWithSoftLock"));
      break;
    case IncognitoLockState::kNone:
      _incognitoReauthPref.value = false;
      _incognitoSoftLockPref.value = false;
      base::UmaHistogramEnumeration(
          kIncognitoLockSettingInteractionHistogram,
          IncognitoLockSettingInteraction::kDoNotHideSelected);
      base::RecordAction(
          base::UserMetricsAction("IOS.Settings.IncognitoLock.DoNotHide"));
      break;
  }
  [self updateConsumer];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateConsumer];
}

#pragma mark - Private

// Updates the consumer with the correct Incognito lock state based on the local
// state prefs for biometric reauth and soft lock.
- (void)updateConsumer {
  if (_incognitoReauthPref.value) {
    [_consumer setIncognitoLockState:IncognitoLockState::kReauth];
  } else if (_incognitoSoftLockPref.value) {
    [_consumer setIncognitoLockState:IncognitoLockState::kSoftLock];
  } else {
    [_consumer setIncognitoLockState:IncognitoLockState::kNone];
  }
}

@end

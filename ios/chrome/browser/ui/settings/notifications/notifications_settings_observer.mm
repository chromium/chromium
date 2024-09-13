// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"

#import <memory>

#import "components/commerce/core/pref_names.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation NotificationsSettingsObserver {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;

  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Pref Service.
  raw_ptr<PrefService> _prefService;

  // Registrar for pref changes in localState.
  PrefChangeRegistrar _localStatePrefChangeRegistrar;

  // Stores Local State.
  raw_ptr<PrefService> _localState;

  // YES if price tracing notification is enabled.
  BOOL _priceTrackingNotificationEnabled;

  // YES if content notification is enabled.
  BOOL _contentNotificationEnabled;

  // YES if sports notification is enabled.
  BOOL _sportsNotificationEnabled;

  // Yes if send tab notification is enabled.
  BOOL _sendTabNotificationEnabled;

  // Yes if tips notification is enabled.
  BOOL _tipsNotificationEnabled;

  // Yes if Safety Check notifications are enabled.
  BOOL _safetyCheckNotificationsEnabled;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                         localState:(PrefService*)localState {
  self = [super init];
  if (self) {
    DCHECK(prefService);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    _prefObserverBridge->ObserveChangesForPreference(
        commerce::kPriceEmailNotificationsEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kFeaturePushNotificationPermissions, &_prefChangeRegistrar);

    _prefService = prefService;
    _priceTrackingNotificationEnabled =
        _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
            .FindBool(kCommerceNotificationKey)
            .value_or(false);
    _contentNotificationEnabled =
        _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
            .FindBool(kContentNotificationKey)
            .value_or(false);
    _contentNotificationEnabled =
        _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
            .FindBool(kSportsNotificationKey)
            .value_or(false);
    _sendTabNotificationEnabled =
        _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
            .FindBool(kSendTabNotificationKey)
            .value_or(false);

    _localStatePrefChangeRegistrar.Init(localState);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kAppLevelPushNotificationPermissions,
        &_localStatePrefChangeRegistrar);

    _localState = localState;
    _tipsNotificationEnabled =
        _localState->GetDict(prefs::kAppLevelPushNotificationPermissions)
            .FindBool(kTipsNotificationKey)
            .value_or(false);
    _safetyCheckNotificationsEnabled =
        _localState->GetDict(prefs::kAppLevelPushNotificationPermissions)
            .FindBool(kSafetyCheckNotificationKey)
            .value_or(false);
  }

  return self;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == commerce::kPriceEmailNotificationsEnabled) {
    [self.delegate notificationsSettingsDidChangeForClient:
                       PushNotificationClientId::kCommerce];
  } else if (preferenceName == prefs::kFeaturePushNotificationPermissions) {
    if (_priceTrackingNotificationEnabled !=
        [self isPriceTrackingNotificationEnabled]) {
      _priceTrackingNotificationEnabled =
          [self isPriceTrackingNotificationEnabled];
      [self.delegate notificationsSettingsDidChangeForClient:
                         PushNotificationClientId::kCommerce];
    } else if (_contentNotificationEnabled !=
               [self isContentNotificationEnabled]) {
      _contentNotificationEnabled = [self isContentNotificationEnabled];
      [self.delegate notificationsSettingsDidChangeForClient:
                         PushNotificationClientId::kContent];
    } else if (_sportsNotificationEnabled !=
               [self isSportsNotificationEnabled]) {
      _sportsNotificationEnabled = [self isSportsNotificationEnabled];
      [self.delegate notificationsSettingsDidChangeForClient:
                         PushNotificationClientId::kSports];
    } else if (_sendTabNotificationEnabled !=
               [self isSendTabNotificationEnabled]) {
      _sendTabNotificationEnabled = [self isSendTabNotificationEnabled];
      [self.delegate notificationsSettingsDidChangeForClient:
                         PushNotificationClientId::kSendTab];
      if (!_sendTabNotificationEnabled) {
        _prefService->SetBoolean(prefs::kSendTabNotificationsPreviouslyDisabled,
                                 true);
      }
    }
  } else if (preferenceName == prefs::kAppLevelPushNotificationPermissions) {
    if (_tipsNotificationEnabled != [self isTipsNotificationEnabled]) {
      _tipsNotificationEnabled = [self isTipsNotificationEnabled];
      [self.delegate notificationsSettingsDidChangeForClient:
                         PushNotificationClientId::kTips];
    } else if (_safetyCheckNotificationsEnabled !=
               [self isSafetyCheckNotificationsEnabled]) {
      _safetyCheckNotificationsEnabled =
          [self isSafetyCheckNotificationsEnabled];
      [self.delegate notificationsSettingsDidChangeForClient:
                         PushNotificationClientId::kSafetyCheck];
    }
  }
}

- (void)disconnect {
  _localStatePrefChangeRegistrar.RemoveAll();
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nullptr;
  _localState = nullptr;
}

#pragma mark - private

- (BOOL)isPriceTrackingNotificationEnabled {
  return _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
      .FindBool(kCommerceNotificationKey)
      .value_or(false);
}

- (BOOL)isContentNotificationEnabled {
  return _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
      .FindBool(kContentNotificationKey)
      .value_or(false);
}

- (BOOL)isSportsNotificationEnabled {
  return _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
      .FindBool(kSportsNotificationKey)
      .value_or(false);
}

- (BOOL)isSendTabNotificationEnabled {
  return _prefService->GetDict(prefs::kFeaturePushNotificationPermissions)
      .FindBool(kSendTabNotificationKey)
      .value_or(false);
}

- (BOOL)isTipsNotificationEnabled {
  return _localState->GetDict(prefs::kAppLevelPushNotificationPermissions)
      .FindBool(kTipsNotificationKey)
      .value_or(false);
}

- (BOOL)isSafetyCheckNotificationsEnabled {
  return _localState->GetDict(prefs::kAppLevelPushNotificationPermissions)
      .FindBool(kSafetyCheckNotificationKey)
      .value_or(false);
}

@end

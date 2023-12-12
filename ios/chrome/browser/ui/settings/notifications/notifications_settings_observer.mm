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

  // YES if price tracing notification is enabled.
  BOOL _priceTrackingNotificationEnabled;

  // YES if content notification is enabled.
  BOOL _contentNotificationEnabled;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
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
    }
  }
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

@end

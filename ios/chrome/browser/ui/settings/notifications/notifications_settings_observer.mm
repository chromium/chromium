// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"

#import <memory>

#import "components/commerce/core/pref_names.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation NotificationsSettingsObserver {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;

  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
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
  }

  return self;
}

#pragma mark - PrefObserverDelegate

// TODO(b/304830588) Decouple kFeaturePushNotificationPermissions from Price
// Tracking to make it universally usable. Add two separate prefs for Content
// and Price Tracking, and keep the original one which is updated if at least
// one of the other prefs is True, and becomes false when both are False.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == commerce::kPriceEmailNotificationsEnabled ||
      preferenceName == prefs::kFeaturePushNotificationPermissions) {
    [self.delegate notificationsSettingsDidChangeForClient:
                       PushNotificationClientId::kCommerce];
  }
}

@end

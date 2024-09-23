// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_SETTINGS_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_SETTINGS_OBSERVER_H_

#import "components/prefs/ios/pref_observer_bridge.h"

class PrefService;
enum class PushNotificationClientId;

// Protocol for receiving updates about Chrome-level push notification
// permission changes.
@protocol NotificationsSettingsObserverDelegate <NSObject>

// Notifies the delegate that the user has updated their notification permission
// settings for `clientID`.
- (void)notificationsSettingsDidChangeForClient:
    (PushNotificationClientId)clientID;

@end

// Listens to prefService updates for each push notification enabled Chrome
// feature and notifies the delegate on changes.
@interface NotificationsSettingsObserver : NSObject <PrefObserverDelegate>

// Delegate to receive a message when notification settings change.
@property(nonatomic, weak) id<NotificationsSettingsObserverDelegate> delegate;

- (instancetype)initWithPrefService:(PrefService*)prefService
                         localState:(PrefService*)localState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Shuts down observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_SETTINGS_OBSERVER_H_

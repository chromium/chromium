// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>
#import <string>
#import <vector>

#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_view_controller_delegate.h"

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

class PrefService;
enum class PushNotificationClientId;
@protocol NotificationsAlertPresenter;
@protocol NotificationsConsumer;
@protocol NotificationsNavigationCommands;

// Mediator for Notifications UI.
@interface NotificationsMediator
    : NSObject <NotificationsSettingsObserverDelegate,
                NotificationsViewControllerDelegate>

// Initializes the mediator with the user's pref service and gaia ID to
// manipulate their push notification permissions.
- (instancetype)initWithPrefService:(PrefService*)prefs
                             gaiaID:(const std::string&)gaiaID
              deviceInfoSyncService:
                  (syncer::DeviceInfoSyncService*)deviceInfoSyncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// View controller.
@property(nonatomic, weak) id<NotificationsConsumer> consumer;

// Handler used to navigate inside the Price Notifications setting.
@property(nonatomic, weak) id<NotificationsNavigationCommands> handler;

// Handler for displaying notification related alerts.
@property(nonatomic, weak) id<NotificationsAlertPresenter> presenter;

// Called after a user disallows notification permissions.
- (void)deniedPermissionsForClientIds:
    (std::vector<PushNotificationClientId>)clientIds;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_MEDIATOR_H_

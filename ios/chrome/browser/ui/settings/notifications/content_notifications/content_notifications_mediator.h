// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_view_controller_delegate.h"

class PrefService;
enum class PushNotificationClientId;
@class TableViewSwitchItem;
class ContentNotificationService;
@protocol NotificationsAlertPresenter;
@protocol ContentNotificationsConsumer;

// Mediator for the Content Notification Settings.
@interface ContentNotificationsMediator
    : NSObject <ContentNotificationsViewControllerDelegate>

// Initializes the mediator with the user's pref service and gaia ID to
// manipulate their push notification permissions.
- (instancetype)initWithPrefService:(PrefService*)prefs
                             gaiaID:(const std::string&)gaiaID
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer for the Content settings menu.
@property(nonatomic, weak) id<ContentNotificationsConsumer> consumer;

// Handler for displaying notification related alerts.
@property(nonatomic, weak) id<NotificationsAlertPresenter> presenter;

// The content notifications service object.
@property(nonatomic, assign) ContentNotificationService* contentNotificationService;

// Called after a user disallows notification permissions.
- (void)deniedPermissionsForClientIds:
    (std::vector<PushNotificationClientId>)clientIds;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_MEDIATOR_H_

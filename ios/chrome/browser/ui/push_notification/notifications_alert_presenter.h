// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_ALERT_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_ALERT_PRESENTER_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"

enum class PushNotificationClientId;

// Protocol for displaying Notification related UIAlerts
@protocol NotificationsAlertPresenter <NSObject>

// Displays the UIAlert that directs the user to the OS permission settings to
// enable push notification permissions when the user toggles Chrome-level push
// notification permissions.
- (void)presentPushNotificationPermissionAlert;

// Displays the UIAlert that directs the user to the OS permission settings to
// enable push notification permissions when the user toggles Chrome-level push
// notification permissions for multiple clients.
@optional
- (void)presentPushNotificationPermissionAlertWithClientIds:
    (std::vector<PushNotificationClientId>)clientIds;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_ALERT_PRESENTER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_PRESENTER_H_

#import <vector>

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_item_identifier.h"

enum class PushNotificationClientId;

// Presenter protocol for the notifications opt-in screen.
@protocol NotificationsOptInPresenter <NSObject>

// Presents the sign in view.
- (void)presentSignIn;

// Presents the notification opt-in alert when the user requests to opt in to
// notifications with client ids of `clientIds`.
- (void)presentNotificationsAlertForClientIds:
    (std::vector<PushNotificationClientId>)clientIds;

// Dismisses the notifications opt-in view.
- (void)dismiss;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_PRESENTER_H_

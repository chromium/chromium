// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_ALERT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_ALERT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

enum class PushNotificationClientId;

// The result of asking for permission to receive notifications.
enum class NotificationsOptInAlertResult {
  kPermissionGranted,
  kPermissionDenied,
  kOpenedSettings,
  kCanceled,
  kError,
};

// A protocol used to communicate the result back to the owning coordinator.
@protocol NotificationsOptInAlertCoordinatorDelegate
// Called with the final result of the opt-in request.
- (void)notificationsOptInAlertResult:(NotificationsOptInAlertResult)result;
@end

// Coordinates the presentation of an alert to ask the user for permission to
// receive notifications. If permission has already been denied, an alert will
// be presented to ask the user if they want to change the app's notification
// permission in iOS settings.
@interface NotificationsOptInAlertCoordinator : ChromeCoordinator

// The delegate which should receive the result of the opt-in request.
@property(nonatomic, weak) id<NotificationsOptInAlertCoordinatorDelegate>
    delegate;

// The client id of the push notification client that the user is
// opting-in for.
@property(nonatomic, assign) PushNotificationClientId clientId;

// The confirmation message to show in a Snackbar when notifications have been
// enabled.
@property(nonatomic, copy) NSString* confirmationMessage;

// The message of the alert. If not set, default message will be used.
@property(nonatomic, copy) NSString* alertMessage;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_ALERT_COORDINATOR_H_

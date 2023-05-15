// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import <string>

#import "ios/chrome/browser/push_notification/push_notification_client_id.h"

// The PushNotificationClient class is an abstract class that provides a
// framework for implementing push notification support. Feature teams that
// intend to support push notifications should create a class that inherits from
// the PushNotificationClient class.
class PushNotificationClient {
 public:
  PushNotificationClient(PushNotificationClientId client_id);
  virtual ~PushNotificationClient() = 0;

  // When the user interacts with a push notification, this function is called
  // to route the user to the appropriate destination.
  virtual void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) = 0;

  // When the device receives a push notification, this function is called to
  // allow the client to process any logic needed at this point in time. The
  // function's return value represents the state of data that the
  // PushNotificationClient fetched.
  virtual UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* user_info) = 0;

  // Actionable Notifications are push notifications that provide the user
  // with predetermined actions that the user can select to manipulate the
  // application without ever entering the application. Actionable
  // notifications must be registered during application startup.
  virtual NSArray<UNNotificationCategory*>*
  RegisterActionableNotifications() = 0;

  // Signals to the client that a browser is ready.
  virtual void OnBrowserReady() = 0;

  // Returns the feature's `client_id_`.
  PushNotificationClientId GetClientId();

 protected:
  // The unique string that is used to associate incoming push notifications to
  // their destination feature. This identifier must match the identifier
  // used inside the notification's payload when sending the notification to the
  // push notification server.
  PushNotificationClientId client_id_;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_H_

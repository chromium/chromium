// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import <memory>
#import <unordered_map>

#import "ios/chrome/browser/push_notification/push_notification_client.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"

class PushNotificationClient;

// A PushNotificationClientManager maintains a list of push notification enabled
// features. The PushNotificationClientManger's purpose is to associate and
// delegate push notifications and its processing logic to the features that own
// the notification. The PushNotificationClientManager routes each notification
// to its appropriate PushNotificationClient based on the incoming
// notification's `push_notification_client_id` property.
class PushNotificationClientManager {
 public:
  PushNotificationClientManager();
  ~PushNotificationClientManager();

  // This function dynamically adds a mapping between a PushNotificationClientId
  // and a PushNotificationClient to the PushNotificationClientManager.
  void AddPushNotificationClient(
      PushNotificationClientId client_id,
      std::unique_ptr<PushNotificationClient> client);

  // This function is called when the user interacts with the delivered
  // notification. This function identifies and delegates the interacted with
  // notification to the appropriate PushNotificationClient.
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response);

  // When a push notification is sent from the server and delivered to the
  // device, UIApplicationDelegate::didReceiveRemoteNotification is invoked.
  // During that invocation, this function is called. This function identifies
  // and delegates the delivered notification to the appropriate
  // PushNotificationClient.
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* user_info);

 private:
  // A list of features that support push notifications.
  std::unordered_map<PushNotificationClientId,
                     std::unique_ptr<PushNotificationClient>>
      clients_ = std::unordered_map<PushNotificationClientId,
                                    std::unique_ptr<PushNotificationClient>>();
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
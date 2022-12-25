// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_

#import "ios/chrome/browser/optimization_guide/optimization_guide_push_notification_client.h"
#import "ios/chrome/browser/push_notification/push_notification_client.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

class CommercePushNotificationClientTest;

class CommercePushNotificationClient
    : public OptimizationGuidePushNotificationClient {
 public:
  CommercePushNotificationClient();
  ~CommercePushNotificationClient() override;

  // Override PushNotificationClient::
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;

 private:
  friend class ::CommercePushNotificationClientTest;

  // Handle the interaction from the user be it tapping the notification or
  // long pressing and then presing 'Visit Site' or 'Untrack Price'.
  void HandleNotificationInteraction(NSString* action_identifier,
                                     NSDictionary* user_info);
};
#endif  // IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_COMMERCE_PUSH_NOTIFICATION_CLIENT_H_

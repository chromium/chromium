// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_PUSH_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

// Client for handling send tab notifications.
class SendTabPushNotificationClient : public PushNotificationClient {
 public:
  SendTabPushNotificationClient();
  ~SendTabPushNotificationClient() override;

  // Override PushNotificationClient.
  bool HandleNotificationInteraction(UNNotificationResponse* response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_PUSH_NOTIFICATION_CLIENT_H_

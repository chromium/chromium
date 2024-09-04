// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

// Client for handling content notifications.
class ContentNotificationClient : public PushNotificationClient {
 public:
  ContentNotificationClient();
  ~ContentNotificationClient() override;

  // Override PushNotificationClient::
  bool HandleNotificationInteraction(UNNotificationResponse* response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* payload) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
};
#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CLIENT_H_

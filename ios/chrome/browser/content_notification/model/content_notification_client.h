// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

class ProfileIOS;

// Client for handling content notifications.
class ContentNotificationClient : public PushNotificationClient {
 public:
  // Constructor for when multi-Profile push notification handling is enabled.
  // Associates this client instance with a specific user `profile`. This should
  // only be called when `IsMultiProfilePushNotificationHandlingEnabled()`
  // returns YES.
  explicit ContentNotificationClient(ProfileIOS* profile);
  ContentNotificationClient();
  ~ContentNotificationClient() override;

  // Override PushNotificationClient::
  bool CanHandleNotification(UNNotification* notification) override;
  bool HandleNotificationInteraction(UNNotificationResponse* response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* payload) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

 private:
  // Stores a notification interaction if the app is not "foreground active"
  // when iOS tells the app about the interaction.
  UNNotificationResponse* stored_interaction_;
};
#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CLIENT_H_

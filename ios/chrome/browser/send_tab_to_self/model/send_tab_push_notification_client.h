// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_PUSH_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

class Browser;
class ProfileIOS;

// Client for handling send tab notifications.
class SendTabPushNotificationClient : public PushNotificationClient {
 public:
  // Constructor for when multi-Profile push notification handling is enabled.
  // Associates this client instance with a specific user `profile`. This should
  // only be called when `IsMultiProfilePushNotificationHandlingEnabled()`
  // returns YES.
  explicit SendTabPushNotificationClient(ProfileIOS* profile);
  // Legacy constructor for when multi-Profile push notification handling is
  // disabled. The client will operate without being tied to a specific Profile.
  SendTabPushNotificationClient();
  ~SendTabPushNotificationClient() override;

  // Override PushNotificationClient.
  std::optional<NotificationType> GetNotificationType(
      UNNotification* notification) override;
  bool CanHandleNotification(UNNotification* notification) override;
  bool HandleNotificationInteraction(UNNotificationResponse* response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;

 private:
  // Handles the completion of URL loads.
  void OnURLLoadedInNewTab(std::string guid, Browser* browser);

  // Weak pointer factory.
  base::WeakPtrFactory<SendTabPushNotificationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_PUSH_NOTIFICATION_CLIENT_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>
#import <optional>

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

class Browser;
enum class TipsNotificationType;

// A notification client responsible for registering notification requests and
// handling the receiving of user notifications that are user-ed "Tips".
class TipsNotificationClient : public PushNotificationClient {
 public:
  TipsNotificationClient();
  ~TipsNotificationClient() override;

  // Override PushNotificationClient::
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

  // Handles a tips notification interaction by opening the appropriate UI.
  void HandleNotificationInteraction(TipsNotificationType type);

 private:
  // Clears any previously requested notification(s).
  void ClearNotification();

  // Request a new tips notification, if the conditions are right (i.e. the
  // user has opted-in, etc).
  void MaybeRequestNotification();

  // Request a notification of the given `type`.
  void RequestNotification(TipsNotificationType type);

  // Returns true if a notification of the given `type` should be sent.
  bool ShouldSendNotification(TipsNotificationType type);

  // Returns the first "foreground active" browser, if any.
  Browser* GetSceneLevelForegroundActiveBrowser();

  // Returns `true` if there is foreground active browser.
  bool IsSceneLevelForegroundActive();

  // Helpers to handle notification interactions.
  void ShowDefaultBrowserPromo();
  void ShowWhatsNew();

  // When the user interacts with a Tips notification but there are no
  // foreground scenes, this will store the notification type so it can
  // be handled when there is a foreground scene.
  std::optional<TipsNotificationType> interacted_type_;
};

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CLIENT_H_

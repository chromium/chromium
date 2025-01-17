// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <optional>

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

// A notification client responsible for scheduling reminder notification
// requests and handling user interactions with reminders.
class ReminderNotificationClient : public PushNotificationClient {
 public:
  ReminderNotificationClient();
  ~ReminderNotificationClient() override;

  // Override PushNotificationClient::
  bool HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

  // Called when the scene becomes "active foreground" and the browser is
  // ready. The closure will be called when all async operations are done.
  void OnSceneActiveForegroundBrowserReady(base::OnceClosure closure);

 private:
  // Used to assert that asynchronous callback are invoked on the correct
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReminderNotificationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_MODEL_REMINDER_NOTIFICATION_CLIENT_H_

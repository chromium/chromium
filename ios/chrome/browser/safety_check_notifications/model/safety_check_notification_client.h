// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_MODEL_SAFETY_CHECK_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_MODEL_SAFETY_CHECK_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "base/sequence_checker.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

// A push notification client for managing Safety Check-related notifications.
// Handles registration, reception, and user interaction with notifications.
class SafetyCheckNotificationClient : public PushNotificationClient {
 public:
  SafetyCheckNotificationClient();
  ~SafetyCheckNotificationClient() override;

  // `PushNotificationClient` overrides.
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

 private:
  // Validates asynchronous `PushNotificationClient` events are evaluated on the
  // same sequence that `SafetyCheckNotificationClient` was created on.
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_MODEL_SAFETY_CHECK_NOTIFICATION_CLIENT_H_

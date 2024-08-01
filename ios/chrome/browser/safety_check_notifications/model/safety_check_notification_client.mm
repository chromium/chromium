// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"

#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"

SafetyCheckNotificationClient::SafetyCheckNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kSafetyCheck) {}

SafetyCheckNotificationClient::~SafetyCheckNotificationClient() = default;

void SafetyCheckNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/356624000): Implement `HandleNotificationInteraction()` to
  // process user interactions with notifications (e.g., taps, dismissals).
}

UIBackgroundFetchResult
SafetyCheckNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/356626805): Implement `HandleNotificationReception()` to log
  // notification receipt for analytics/debugging.

  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
SafetyCheckNotificationClient::RegisterActionableNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/356624328): Implement actionable Safety Check notifications
  // allowing users to take direct actions from the notification.

  return @[];
}

void SafetyCheckNotificationClient::OnSceneActiveForegroundBrowserReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/347975105): Implement
  // `OnSceneActiveForegroundBrowserReady()` to conditionally schedule
  // notifications.
}

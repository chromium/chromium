// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"

ReminderNotificationClient::ReminderNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kReminders) {}

ReminderNotificationClient::~ReminderNotificationClient() = default;

bool ReminderNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/390432325): Handle reminder notification interactions.
  return false;
}

std::optional<UIBackgroundFetchResult>
ReminderNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* userInfo) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::nullopt;
}

NSArray<UNNotificationCategory*>*
ReminderNotificationClient::RegisterActionableNotifications() {
  return @[];
}

void ReminderNotificationClient::OnSceneActiveForegroundBrowserReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnSceneActiveForegroundBrowserReady(base::DoNothing());
}

void ReminderNotificationClient::OnSceneActiveForegroundBrowserReady(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/390432325): Handle reminder notification interactions.
  std::move(closure).Run();
}

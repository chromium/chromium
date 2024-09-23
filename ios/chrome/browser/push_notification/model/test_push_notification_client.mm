// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/test_push_notification_client.h"

#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"

TestPushNotificationClient::TestPushNotificationClient(size_t client_id)
    : PushNotificationClient(static_cast<PushNotificationClientId>(client_id)) {
}
TestPushNotificationClient::~TestPushNotificationClient() = default;

bool TestPushNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* notification) {
  has_notification_received_interaction_ = true;
  return false;
}

std::optional<UIBackgroundFetchResult>
TestPushNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  return fetch_result_;
}

NSArray<UNNotificationCategory*>*
TestPushNotificationClient::RegisterActionableNotifications() {
  // Add actional notifications as new notification types are added.
  return @[];
}

bool TestPushNotificationClient::HasNotificationReceivedInteraction() {
  return has_notification_received_interaction_;
}

void TestPushNotificationClient::SetBackgroundFetchResult(
    std::optional<UIBackgroundFetchResult> result) {
  fetch_result_ = result;
}

void TestPushNotificationClient::OnSceneActiveForegroundBrowserReady() {
  is_browser_ready_ = true;
}

bool TestPushNotificationClient::IsBrowserReady() {
  return is_browser_ready_;
}

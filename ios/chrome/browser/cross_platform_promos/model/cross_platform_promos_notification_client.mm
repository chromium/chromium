// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_notification_client.h"

#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"

CrossPlatformPromosNotificationClient::CrossPlatformPromosNotificationClient(
    ProfileIOS* profile)
    : PushNotificationClient(PushNotificationClientId::kCrossPlatformPromos,
                             profile) {}

CrossPlatformPromosNotificationClient::
    ~CrossPlatformPromosNotificationClient() = default;

bool CrossPlatformPromosNotificationClient::CanHandleNotification(
    UNNotification* notification) {
  // TODO(crbug.com/445676018): Handle notification interactions.
  return false;
}

std::optional<UIBackgroundFetchResult>
CrossPlatformPromosNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* user_info) {
  // TODO(crbug.com/445676018): Handle notification interactions.
  return std::nullopt;
}

bool CrossPlatformPromosNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  // TODO(crbug.com/445676018): Handle notification interactions.
  return false;
}

std::optional<NotificationType>
CrossPlatformPromosNotificationClient::GetNotificationType(
    UNNotification* notification) {
  // TODO(crbug.com/445676018): Handle notification interactions.
  return std::nullopt;
}

void CrossPlatformPromosNotificationClient::
    OnSceneActiveForegroundBrowserReady() {
  // TODO(crbug.com/445676018): Handle notification interactions.
}

NSArray<UNNotificationCategory*>*
CrossPlatformPromosNotificationClient::RegisterActionableNotifications() {
  return nil;
}

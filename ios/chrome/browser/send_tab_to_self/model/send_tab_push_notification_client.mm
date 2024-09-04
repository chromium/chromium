// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"

#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

SendTabPushNotificationClient::SendTabPushNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kSendTab) {}

SendTabPushNotificationClient::~SendTabPushNotificationClient() = default;

bool SendTabPushNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  // TODO(crbug.com/343495218): Load URL in new tab and dismiss Send Tab
  // infobar.
  return false;
}

std::optional<UIBackgroundFetchResult>
SendTabPushNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  // TODO(crbug.com/343495218): Handle notification reception.
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
SendTabPushNotificationClient::RegisterActionableNotifications() {
  return @[];
}

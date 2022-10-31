// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/commerce_push_notification_client.h"

#import "ios/chrome/browser/push_notification/push_notification_client_id.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

CommercePushNotificationClient::CommercePushNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kCommerce) {}
CommercePushNotificationClient::~CommercePushNotificationClient() = default;

void CommercePushNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* notification) {
  // TODO(crbug.com/1362341) handle taking user to the shopping
  // website corresponding to their notification.
  // TODO(crbug.com/1362342) handle the user clicking 'untrack price'.
}

UIBackgroundFetchResult
CommercePushNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
CommercePushNotificationClient::RegisterActionableNotifications() {
  // Add actional notifications as new notification types are added.
  return @[];
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_client.h"

#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "url/gurl.h"

ContentNotificationClient::ContentNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kContent) {}

ContentNotificationClient::~ContentNotificationClient() = default;

void ContentNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  NSDictionary<NSString*, id>* payload =
      response.notification.request.content.userInfo;

  ContentNotificationService* contentNotificationService =
      ContentNotificationServiceFactory::GetForBrowserState(
          GetLastUsedBrowserState());

  const GURL& url = contentNotificationService->GetDestinationUrl(payload);
  loadUrlInNewTab(url);
}

UIBackgroundFetchResult ContentNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* payload) {
  // TODO: b/332578232 - Implement notification reception logic.
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
ContentNotificationClient::RegisterActionableNotifications() {
  // TODO: b/332578232 - Register actionalbe notifications.
  return @[];
}

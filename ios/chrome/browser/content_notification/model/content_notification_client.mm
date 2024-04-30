// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_client.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
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
  if ([response.actionIdentifier
          isEqualToString:kContentNotificationFeedbackActionIdentifier]) {
    NSDictionary<NSString*, NSString*>* feedbackPayload =
        contentNotificationService->GetFeedbackPayload(payload);
    loadFeedbackWithPayloadAndClientId(feedbackPayload,
                                       PushNotificationClientId::kContent);
  } else {
    const GURL& url = contentNotificationService->GetDestinationUrl(payload);
    if (url.is_empty()) {
      base::UmaHistogramBoolean("ContentNotifications.OpenURLAction.HasURL",
                                false);
      loadUrlInNewTab(GURL("chrome://newtab"));
    }
    base::UmaHistogramBoolean("ContentNotifications.OpenURLAction.HasURL",
                              true);
    loadUrlInNewTab(url);
  }
}

UIBackgroundFetchResult ContentNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* payload) {
  // TODO: b/332578232 - Implement notification reception logic.
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
ContentNotificationClient::RegisterActionableNotifications() {
  UNNotificationAction* feedbackAction = [UNNotificationAction
      actionWithIdentifier:kContentNotificationFeedbackActionIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_CONTENT_NOTIFICATIONS_SEND_FEEDBACK)
                   options:UNNotificationActionOptionForeground];
  UNNotificationCategory* contentNotificationCategory = [UNNotificationCategory
      categoryWithIdentifier:kContentNotificationFeedbackCategoryIdentifier
                     actions:@[ feedbackAction ]
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionCustomDismissAction];
  return @[ contentNotificationCategory ];
}

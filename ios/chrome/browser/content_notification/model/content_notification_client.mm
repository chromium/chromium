// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_client.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/content_notification/model/content_notification_nau_configuration.h"
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
  // In order to send delivered NAUs, the payload has been modified for it to be
  // processed on `HandleNotificationReception()`. Before reusing the payload,
  // remove the NAU body paramater from the payload to return it to its normal
  // state.
  NSMutableDictionary<NSString*, id>* unprocessedPayload =
      [response.notification.request.content.userInfo mutableCopy];
  if ([unprocessedPayload objectForKey:kContentNotificationNAUBodyParameter]) {
    [unprocessedPayload
        removeObjectForKey:kContentNotificationNAUBodyParameter];
  }
  // Regenerate the regular payload as NSDictionary after removing the extra
  // object.
  NSDictionary<NSString*, id>* payload = [unprocessedPayload copy];
  ContentNotificationService* contentNotificationService =
      ContentNotificationServiceFactory::GetForBrowserState(
          GetLastUsedBrowserState());
  ContentNotificationNAUConfiguration* config =
      [[ContentNotificationNAUConfiguration alloc] init];
  config.notification = response.notification;
  if ([response.actionIdentifier
          isEqualToString:kContentNotificationFeedbackActionIdentifier]) {
    config.actionType = NAUActionTypeFeedbackClicked;
    NSDictionary<NSString*, NSString*>* feedbackPayload =
        contentNotificationService->GetFeedbackPayload(payload);
    loadFeedbackWithPayloadAndClientId(feedbackPayload,
                                       PushNotificationClientId::kContent);
  } else if ([response.actionIdentifier
                 isEqualToString:UNNotificationDefaultActionIdentifier]) {
    config.actionType = NAUActionTypeOpened;
    const GURL& url = contentNotificationService->GetDestinationUrl(payload);
    if (url.is_empty()) {
      base::UmaHistogramBoolean("ContentNotifications.OpenURLAction.HasURL",
                                false);
      loadUrlInNewTab(GURL("chrome://newtab"));
    }
    base::UmaHistogramBoolean("ContentNotifications.OpenURLAction.HasURL",
                              true);
    loadUrlInNewTab(url);
  } else if ([response.actionIdentifier
                 isEqualToString:UNNotificationDismissActionIdentifier]) {
    config.actionType = NAUActionTypeDismissed;
  }
  // TODO(crbug.com/337871560): Three way patch NAU and adding completion
  // handler.
  contentNotificationService->SendNAUForConfiguration(config);
}

// TODO(crbug.com/338875261): Add background refresh support.
// Log a Delivered NAU from here. Only works when not in the background. This
// method doesn't modify the payload and doesn't pass it forward, it is
// readonly.
UIBackgroundFetchResult ContentNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* payload) {
  ContentNotificationService* contentNotificationService =
      ContentNotificationServiceFactory::GetForBrowserState(
          GetLastUsedBrowserState());
  NSString* notificationBody =
      [payload objectForKey:kContentNotificationNAUBodyParameter];
  if (notificationBody) {
    // Send NAU.
    ContentNotificationNAUConfiguration* config =
        [[ContentNotificationNAUConfiguration alloc] init];
    config.actionType = NAUActionTypeDisplayed;
    // Create a new payload without the parameter to mimic the original payload,
    // to be sent with the NAU.
    NSMutableDictionary<NSString*, id>* newPayload = [payload mutableCopy];
    [newPayload removeObjectForKey:kContentNotificationNAUBodyParameter];
    UNMutableNotificationContent* content =
        [[UNMutableNotificationContent alloc] init];
    content.body = notificationBody;
    content.userInfo = [newPayload copy];
    config.content = content;
    contentNotificationService->SendNAUForConfiguration(config);
  }
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

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

bool ContentNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  // Need to check if it is a content notification first to avoid conflicts with
  // other clients.
  if (![response.notification.request.content.categoryIdentifier
          isEqualToString:kContentNotificationFeedbackCategoryIdentifier]) {
    return false;
  }
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
      ContentNotificationServiceFactory::GetForProfile(GetAnyProfile());
  ContentNotificationNAUConfiguration* config =
      [[ContentNotificationNAUConfiguration alloc] init];
  config.notification = response.notification;
  if ([response.actionIdentifier
          isEqualToString:kContentNotificationFeedbackActionIdentifier]) {
    config.actionType = NAUActionTypeFeedbackClicked;
    base::UmaHistogramEnumeration(
        kContentNotificationActionHistogramName,
        NotificationActionType::kNotificationActionTypeFeedbackClicked);
    NSDictionary<NSString*, NSString*>* feedbackPayload =
        contentNotificationService->GetFeedbackPayload(payload);
    LoadFeedbackWithPayloadAndClientId(feedbackPayload,
                                       PushNotificationClientId::kContent);
  } else if ([response.actionIdentifier
                 isEqualToString:UNNotificationDefaultActionIdentifier]) {
    config.actionType = NAUActionTypeOpened;
    base::UmaHistogramEnumeration(
        kContentNotificationActionHistogramName,
        NotificationActionType::kNotificationActionTypeOpened);
    const GURL& url = contentNotificationService->GetDestinationUrl(payload);
    if (url.is_empty()) {
      base::UmaHistogramBoolean("ContentNotifications.OpenURLAction.HasURL",
                                false);
      return true;
    }
    base::UmaHistogramBoolean("ContentNotifications.OpenURLAction.HasURL",
                              true);
    LoadUrlInNewTab(url);
  } else if ([response.actionIdentifier
                 isEqualToString:UNNotificationDismissActionIdentifier]) {
    base::UmaHistogramBoolean("ContentNotifications.DismissAction", true);
    config.actionType = NAUActionTypeDismissed;
    base::UmaHistogramEnumeration(
        kContentNotificationActionHistogramName,
        NotificationActionType::kNotificationActionTypeDismissed);
  }
  // TODO(crbug.com/337871560): Three way patch NAU and adding completion
  // handler.
  contentNotificationService->SendNAUForConfiguration(config);
  return true;
}

// TODO(crbug.com/338875261): Add background refresh support.
// Delivered NAUs are currently being sent from the push_notification_delegate,
// and in the future they should be here once background refresh is available.
std::optional<UIBackgroundFetchResult>
ContentNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* payload) {
  return std::nullopt;
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

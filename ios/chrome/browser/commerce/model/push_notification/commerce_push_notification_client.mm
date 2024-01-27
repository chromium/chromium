// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/push_notification/commerce_push_notification_client.h"

#import "base/base64.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/optimization_guide/core/hints_manager.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "url/gurl.h"

namespace {

// Identifier for long press on notification and open menu categories.
NSString* kCommerceCategoryIdentifier = @"PriceDropNotifications";
// Identifier if user taps notification (doesn't long press and
// choose from options).
NSString* kDefaultActionIdentifier =
    @"com.apple.UNNotificationDefaultActionIdentifier";
// Opaque payload key from notification service.
NSString* kSerializedPayloadKey = @"op";
// Identifier for user pressing 'Visit site' option after long pressing
// notification.
NSString* kVisitSiteActionIdentifier = @"visit_site";
// Text for option for long press.
NSString* kVisitSiteTitle = @"Visit site";
// Identifier for user pressing 'Untrack price' after long pressing
// notification.
NSString* kUntrackPriceIdentifier = @"untrack_price";
// Text for option 'Untrack price' when long pressing notification.
NSString* kUntrackPriceTitle = @"Untrack price";

}  // namespace

CommercePushNotificationClient::CommercePushNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kCommerce) {}

CommercePushNotificationClient::~CommercePushNotificationClient() = default;

// static
std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
CommercePushNotificationClient::ParseHintNotificationPayload(
    NSString* serialized_payload_escaped) {
  std::string serialized_payload_unescaped;
  if (!base::Base64Decode(base::SysNSStringToUTF8(serialized_payload_escaped),
                          &serialized_payload_unescaped)) {
    return nullptr;
  }
  optimization_guide::proto::Any any;
  if (!any.ParseFromString(serialized_payload_unescaped) || !any.has_value()) {
    return nullptr;
  }
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload = std::make_unique<
          optimization_guide::proto::HintNotificationPayload>();
  if (!hint_notification_payload->ParseFromString(any.value())) {
    return nullptr;
  }
  return hint_notification_payload;
}

void CommercePushNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* notification_response) {
  NSDictionary* user_info =
      notification_response.notification.request.content.userInfo;
  DCHECK(user_info);
  HandleNotificationInteraction(notification_response.actionIdentifier,
                                user_info);
}

UIBackgroundFetchResult
CommercePushNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.PushNotification.Received"));
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForBrowserState(
          GetLastUsedBrowserState());
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload = ParseHintNotificationPayload(
          [notification objectForKey:kSerializedPayloadKey]);
  if (hint_notification_payload) {
    optimization_guide::PushNotificationManager* push_notification_manager =
        optimization_guide_service->GetHintsManager()
            ->push_notification_manager();
    push_notification_manager->OnNewPushNotification(
        *hint_notification_payload);
  }
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
CommercePushNotificationClient::RegisterActionableNotifications() {
  UNNotificationAction* kVisitSiteAction = [UNNotificationAction
      actionWithIdentifier:kVisitSiteActionIdentifier
                     title:kVisitSiteTitle
                   options:UNNotificationActionOptionForeground];
  UNNotificationAction* kUntrackPriceAction = [UNNotificationAction
      actionWithIdentifier:kUntrackPriceIdentifier
                     title:kUntrackPriceTitle
                   options:UNNotificationActionOptionForeground];

  return @[ [UNNotificationCategory
      categoryWithIdentifier:kCommerceCategoryIdentifier
                     actions:@[ kVisitSiteAction, kUntrackPriceAction ]
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionNone] ];
}

commerce::ShoppingService*
CommercePushNotificationClient::GetShoppingService() {
  return commerce::ShoppingServiceFactory::GetForBrowserState(
      GetLastUsedBrowserState());
}

bookmarks::BookmarkModel* CommercePushNotificationClient::GetBookmarkModel() {
  return ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
      GetLastUsedBrowserState());
}

void CommercePushNotificationClient::HandleNotificationInteraction(
    NSString* action_identifier,
    NSDictionary* user_info,
    base::RunLoop* on_complete_for_testing) {
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload =
          CommercePushNotificationClient::ParseHintNotificationPayload(
              [user_info objectForKey:kSerializedPayloadKey]);
  if (!hint_notification_payload) {
    return;
  }

  commerce::PriceDropNotificationPayload price_drop_notification;
  if (!hint_notification_payload->has_payload() ||
      !price_drop_notification.ParseFromString(
          hint_notification_payload->payload().value())) {
    return;
  }

  // TODO(crbug.com/1362342) handle the user tapping 'untrack price'.
  // User taps notification or long presses notification and presses 'Visit
  // Site'.
  if ([action_identifier isEqualToString:kVisitSiteActionIdentifier] ||
      [action_identifier isEqualToString:kDefaultActionIdentifier]) {
    if ([action_identifier isEqualToString:kVisitSiteActionIdentifier]) {
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.PushNotification.VisitSiteTapped"));
    } else if ([action_identifier isEqualToString:kDefaultActionIdentifier]) {
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.PushNotification.NotificationTapped"));
    }
    loadUrlInNewTab(GURL(price_drop_notification.destination_url()));
  } else if ([action_identifier isEqualToString:kUntrackPriceIdentifier]) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.PushNotification.UnTrackProductTapped"));

    const bookmarks::BookmarkNode* bookmark =
        GetBookmarkModel()->GetMostRecentlyAddedUserNodeForURL(
            GURL(price_drop_notification.destination_url()));
    base::UmaHistogramBoolean("Commerce.PriceTracking.Untrack.BookmarkFound",
                              bookmark != nil);
    if (!bookmark) {
      if (on_complete_for_testing) {
        on_complete_for_testing->Quit();
      }
      return;
    }
    commerce::SetPriceTrackingStateForBookmark(
        GetShoppingService(), GetBookmarkModel(), bookmark, false,
        base::BindOnce(^(bool success) {
          if (on_complete_for_testing) {
            on_complete_for_testing->Quit();
          }
          base::UmaHistogramBoolean("Commerce.PriceTracking.Untrack.Success",
                                    success);
        }));
  }
}

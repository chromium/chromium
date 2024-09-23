// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/push_notification/commerce_push_notification_client.h"

#import "base/base64.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/optimization_guide/core/hints_manager.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
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

bool CommercePushNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* notification_response) {
  NSDictionary* user_info =
      notification_response.notification.request.content.userInfo;
  DCHECK(user_info);
  return HandleNotificationInteraction(notification_response.actionIdentifier,
                                       user_info, base::DoNothing());
}

std::optional<UIBackgroundFetchResult>
CommercePushNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(GetAnyProfile());
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload = ParseHintNotificationPayload(
          [notification objectForKey:kSerializedPayloadKey]);
  if (hint_notification_payload) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.PushNotification.Received"));
    optimization_guide::PushNotificationManager* push_notification_manager =
        optimization_guide_service->GetHintsManager()
            ->push_notification_manager();
    push_notification_manager->OnNewPushNotification(
        *hint_notification_payload);
    return UIBackgroundFetchResultNoData;
  }
  return std::nullopt;
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
  return commerce::ShoppingServiceFactory::GetForBrowserState(GetAnyProfile());
}

bookmarks::BookmarkModel* CommercePushNotificationClient::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(GetAnyProfile());
}

bool CommercePushNotificationClient::HandleNotificationInteraction(
    NSString* action_identifier,
    NSDictionary* user_info,
    base::OnceClosure completion) {
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload =
          CommercePushNotificationClient::ParseHintNotificationPayload(
              [user_info objectForKey:kSerializedPayloadKey]);
  if (!hint_notification_payload) {
    std::move(completion).Run();
    return false;
  }

  commerce::PriceDropNotificationPayload price_drop_notification;
  if (!hint_notification_payload->has_payload() ||
      !price_drop_notification.ParseFromString(
          hint_notification_payload->payload().value())) {
    std::move(completion).Run();
    return false;
  }

  // TODO(crbug.com/40238314) handle the user tapping 'untrack price'.
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
    LoadUrlInNewTab(GURL(price_drop_notification.destination_url()));
  } else if ([action_identifier isEqualToString:kUntrackPriceIdentifier]) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.PushNotification.UnTrackProductTapped"));

    const bookmarks::BookmarkNode* bookmark =
        GetBookmarkModel()->GetMostRecentlyAddedUserNodeForURL(
            GURL(price_drop_notification.destination_url()));
    base::UmaHistogramBoolean("Commerce.PriceTracking.Untrack.BookmarkFound",
                              bookmark != nil);
    if (!bookmark) {
      std::move(completion).Run();
      return true;
    }
    commerce::SetPriceTrackingStateForBookmark(
        GetShoppingService(), GetBookmarkModel(), bookmark, false,
        base::BindOnce([](bool success) {
          base::UmaHistogramBoolean("Commerce.PriceTracking.Untrack.Success",
                                    success);
        }).Then(std::move(completion)));
  }
  return true;
}

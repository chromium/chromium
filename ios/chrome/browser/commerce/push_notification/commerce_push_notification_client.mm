// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/commerce_push_notification_client.h"

#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
NSString* kVisitSiteTitle = @"Visit Site";

}  // namespace

CommercePushNotificationClient::CommercePushNotificationClient()
    : OptimizationGuidePushNotificationClient(
          PushNotificationClientId::kCommerce) {}

CommercePushNotificationClient::~CommercePushNotificationClient() = default;

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
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
CommercePushNotificationClient::RegisterActionableNotifications() {
  UNNotificationAction* kVisitSiteAction = [UNNotificationAction
      actionWithIdentifier:kVisitSiteActionIdentifier
                     title:kVisitSiteTitle
                   options:UNNotificationActionOptionForeground];
  return @[ [UNNotificationCategory
      categoryWithIdentifier:kCommerceCategoryIdentifier
                     actions:@[ kVisitSiteAction ]
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionNone] ];
}

void CommercePushNotificationClient::HandleNotificationInteraction(
    NSString* action_identifier,
    NSDictionary* user_info) {
  // TODO(crbug.com/1362342) handle the user tapping 'untrack price'.
  // User taps notification or long presses notification and presses 'Visit
  // Site'.
  if ([action_identifier isEqualToString:kVisitSiteActionIdentifier] ||
      [action_identifier isEqualToString:kDefaultActionIdentifier]) {
    // TODO(crbug.com/1403190) implement alternate Open URL handler which
    // attempts to find if a Tab with the URL already exists and switch
    // to that Tab.
    std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
        hint_notification_payload = OptimizationGuidePushNotificationClient::
            ParseHintNotificationPayload(
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

    BrowserList* browser_list =
        BrowserListFactory::GetForBrowserState(GetLastUsedBrowserState());
    if (!browser_list->AllRegularBrowsers().size()) {
      return;
    }
    // TODO(crbug.com/1403199) find first foregrounded browser instead of simply
    // first browser here.
    Browser* browser = *browser_list->AllRegularBrowsers().begin();
    UrlLoadParams params = UrlLoadParams::InNewTab(
        GURL(price_drop_notification.destination_url()));
    UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);
  }
}

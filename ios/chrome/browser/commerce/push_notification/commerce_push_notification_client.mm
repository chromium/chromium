// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/commerce_push_notification_client.h"

#import "base/metrics/histogram_functions.h"
#import "base/run_loop.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
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
NSString* kVisitSiteTitle = @"Visit site";
// Identifier for user pressing 'Untrack price' after long pressing
// notification.
NSString* kUntrackPriceIdentifier = @"untrack_price";
// Text for option 'Untrack price' when long pressing notification.
NSString* kUntrackPriceTitle = @"Untrack price";

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

void CommercePushNotificationClient::OnSceneActiveForegroundBrowserReady() {
  if (!urls_delayed_for_loading_.size()) {
    return;
  }
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  CHECK(browser);
  for (const std::string& url : urls_delayed_for_loading_) {
    UrlLoadParams params = UrlLoadParams::InNewTab(GURL(url));
    UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);
  }
  urls_delayed_for_loading_.clear();
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

Browser*
CommercePushNotificationClient::GetSceneLevelForegroundActiveBrowser() {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(GetLastUsedBrowserState());
  for (Browser* browser : browser_list->AllRegularBrowsers()) {
    if (!browser->IsInactive()) {
      SceneStateBrowserAgent* scene_state_browser_agent =
          SceneStateBrowserAgent::FromBrowser(browser);
      if (scene_state_browser_agent &&
          scene_state_browser_agent->GetSceneState() &&
          scene_state_browser_agent->GetSceneState().activationLevel ==
              SceneActivationLevelForegroundActive) {
        return browser;
      }
    }
  }
  return nullptr;
}

void CommercePushNotificationClient::HandleNotificationInteraction(
    NSString* action_identifier,
    NSDictionary* user_info,
    base::RunLoop* on_complete_for_testing) {
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload =
          OptimizationGuidePushNotificationClient::ParseHintNotificationPayload(
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
    // TODO(crbug.com/1403190) implement alternate Open URL handler which
    // attempts to find if a Tab with the URL already exists and switch
    // to that Tab.
    Browser* browser = GetSceneLevelForegroundActiveBrowser();
    if (!browser) {
      urls_delayed_for_loading_.push_back(
          price_drop_notification.destination_url());
      return;
    }
    // TODO(crbug.com/1403199) find first foregrounded browser instead of simply
    // first browser here.
    UrlLoadParams params = UrlLoadParams::InNewTab(
        GURL(price_drop_notification.destination_url()));
    UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);
  } else if ([action_identifier isEqualToString:kUntrackPriceIdentifier]) {
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

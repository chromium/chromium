// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/price_notifications/price_notifications_tab_helper.h"

#import "components/commerce/core/shopping_service.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"

namespace {

void OnProductInfoUrl(
    id<HelpCommands> help_handler,
    const GURL& product_url,
    const std::optional<const commerce::ProductInfo>& product_info) {
  if (!product_info) {
    return;
  }
  [help_handler presentInProductHelpWithType:
                    InProductHelpType::kPriceNotificationsWhileBrowsing];
}

// Returns whether the price notification should be presented
// for `web_state`.
bool ShouldPresentPriceNotifications(web::WebState* web_state) {
  ProfileIOS* const profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());

  if (!IsPriceTrackingEnabled(profile)) {
    return false;
  }

  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  if (!tracker->WouldTriggerHelpUI(
          feature_engagement ::kIPHPriceNotificationsWhileBrowsingFeature)) {
    return false;
  }

  return true;
}

// Records a visit to a website that is eligible for price tracking.
// This allows the Tips Manager to provide relevant tips or guidance
// to the user about the price tracking feature.
void RecordPriceTrackableSiteVisit(web::WebState* web_state) {
  CHECK(IsSegmentationTipsManagerEnabled());

  ProfileIOS* const profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());

  TipsManagerIOS* tipsManager = TipsManagerIOSFactory::GetForProfile(profile);

  tipsManager->NotifySignal(
      segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite);
}

}  // namespace

PriceNotificationsTabHelper::PriceNotificationsTabHelper(
    web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
  shopping_service_ = commerce::ShoppingServiceFactory::GetForBrowserState(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
}

PriceNotificationsTabHelper::~PriceNotificationsTabHelper() = default;

void PriceNotificationsTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Do not show price notifications IPH if the feature engagement
  // conditions are not fulfilled.
  if (!ShouldPresentPriceNotifications(web_state)) {
    return;
  }

  if (IsSegmentationTipsManagerEnabled()) {
    RecordPriceTrackableSiteVisit(web_state);
  }

  // Local strong reference for binding to the callback below.
  id<HelpCommands> help_handler = help_handler_;
  shopping_service_->GetProductInfoForUrl(
      web_state->GetVisibleURL(),
      base::BindOnce(&OnProductInfoUrl, help_handler));
}

void PriceNotificationsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(PriceNotificationsTabHelper)

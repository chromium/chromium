// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/price_notifications/price_notifications_tab_helper.h"

#import "components/commerce/core/shopping_service.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/commerce/price_notifications/price_notifications_iph_presenter.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Helper object to weakly bind `presenter` in the callback.
@interface WeakPriceNotificationsPresenter : NSObject
- (instancetype)initWithPresenter:(id<PriceNotificationsIPHPresenter>)presenter
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PriceNotificationsIPHPresenter> presenter;
@end

@implementation WeakPriceNotificationsPresenter
- (instancetype)initWithPresenter:
    (id<PriceNotificationsIPHPresenter>)presenter {
  if ((self = [super init])) {
    _presenter = presenter;
  }

  return self;
}
@end

namespace {

void OnProductInfoUrl(
    WeakPriceNotificationsPresenter* presenter,
    const GURL& product_url,
    const absl::optional<commerce::ProductInfo>& product_info) {
  DCHECK(presenter);
  if (!product_info) {
    return;
  }

  [presenter.presenter presentPriceNotificationsWhileBrowsingIPH];
}

// Returns whether the price notification should be presented
// for `web_state`.
bool ShouldPresentPriceNotifications(web::WebState* web_state) {
  ChromeBrowserState* const browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());

  if (!IsPriceTrackingEnabled(browser_state)) {
    return false;
  }

  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browser_state);
  if (!tracker->WouldTriggerHelpUI(
          feature_engagement ::kIPHPriceNotificationsWhileBrowsingFeature)) {
    return false;
  }

  return true;
}

}  // namespace

PriceNotificationsTabHelper::PriceNotificationsTabHelper(
    web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
  shopping_service_ = commerce::ShoppingServiceFactory::GetForBrowserState(
      web_state->GetBrowserState());
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

  WeakPriceNotificationsPresenter* weak_presenter =
      [[WeakPriceNotificationsPresenter alloc]
          initWithPresenter:price_notifications_iph_presenter_];
  shopping_service_->GetProductInfoForUrl(
      web_state->GetVisibleURL(),
      base::BindOnce(&OnProductInfoUrl, weak_presenter));
}

void PriceNotificationsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(PriceNotificationsTabHelper)

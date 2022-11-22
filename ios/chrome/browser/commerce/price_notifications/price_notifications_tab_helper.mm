// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/price_notifications/price_notifications_tab_helper.h"

#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/shopping_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void OnProductInfoUrl(
    const GURL& productURL,
    const absl::optional<commerce::ProductInfo>& product_info) {
  if (!product_info)
    return;

  // TODO(crbug.com/1362350) Once the PriceNotificationsTabHelper
  // has landed, this section will display the IPH.
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
  shopping_service_->GetProductInfoForUrl(web_state->GetVisibleURL(),
                                          base::BindOnce(&OnProductInfoUrl));
}

void PriceNotificationsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(PriceNotificationsTabHelper)

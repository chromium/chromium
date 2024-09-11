// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

@class PriceTrackingPromoItem;

namespace commerce {
class ShoppingService;
}  // namespace commerce

// Category for exposing internal state for testing.
@interface PriceTrackingPromoMediator (ForTesting)

- (commerce::ShoppingService*)shoppingServiceForTesting;

- (PriceTrackingPromoItem*)priceTrackingPromoItemForTesting;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_

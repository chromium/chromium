// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_DELEGATE_H_

// Delegate used to communicate events back to the owner of
// PriceTrackingPromoMediator.
@protocol PriceTrackingPromoMediatorDelegate

// New subscription for user observed (originating from a different platform).
- (void)newSubscriptionAvailable;

// Price Tracking Promo is removed from the magic stack.
- (void)removePriceTrackingPromo;

// Price Tracking promo was tapped on.
- (void)promoWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_DELEGATE_H_

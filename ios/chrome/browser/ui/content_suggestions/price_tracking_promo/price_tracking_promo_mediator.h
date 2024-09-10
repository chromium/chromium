// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_commands.h"

namespace commerce {
class ShoppingService;
}

@class PriceTrackingPromoItem;

// Delegate used to communicate events back to the owner of
// PriceTrackingPromoMediator.
@protocol PriceTrackingPromoMediatorDelegate

// New subscription for user observed (originating from a different platform).
- (void)newSubscriptionAvailable;

// Price Tracking Promo is removed from the magic stack.
- (void)removePriceTrackingPromo;

@end

@interface PriceTrackingPromoMediator : NSObject <PriceTrackingPromoCommands>

// Default initializer.
- (instancetype)initWithShoppingService:
    (commerce::ShoppingService*)shoppingService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Resets the latest fetched subscriptions and re-fetches if applicable.
- (void)reset;

// Fetches the most recent subscription for the user.
- (void)fetchLatestSubscription;

// Disables and hides the price tracking promo module.
- (void)disableModule;

// Data for price tracking promo to show. Includes the image for the
// latest subscription to be displayed.
- (PriceTrackingPromoItem*)priceTrackingPromoItemToShow;

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<PriceTrackingPromoMediatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_H_

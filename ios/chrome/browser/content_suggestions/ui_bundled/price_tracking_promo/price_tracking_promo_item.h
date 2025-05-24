// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ITEM_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module.h"

@protocol PriceTrackingPromoCommands;
@protocol PriceTrackingPromoFaviconConsumerSource;

// Item containing the configurations for the Price Tracking Promo Module view.
@interface PriceTrackingPromoItem : MagicStackModule

// Command handler for user actions.
@property(nonatomic, weak) id<PriceTrackingPromoCommands> commandHandler;

// Serialized image data for the most recent price tracked product image.
@property(nonatomic, strong) NSData* productImageData;

// The favicon image of the product if any.
@property(nonatomic, strong) UIImage* faviconImage;

// The consumer source of the favicon image for the product.
@property(nonatomic, strong) id<PriceTrackingPromoFaviconConsumerSource>
    priceTrackingPromoFaviconConsumerSource;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ITEM_H_

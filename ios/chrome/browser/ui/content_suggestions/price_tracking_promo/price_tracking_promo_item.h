// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ITEM_H_

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

@protocol PriceTrackingPromoCommands;

// Item containing the configurations for the Price Tracking Promo Module view.
@interface PriceTrackingPromoItem : MagicStackModule

// Command handler for user actions.
@property(nonatomic, weak) id<PriceTrackingPromoCommands> commandHandler;

// Serialized image data for the most recent price tracked product image.
@property(nonatomic, strong) NSData* productImageData;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_ITEM_H_

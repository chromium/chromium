// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_ITEM_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view_config.h"

@protocol PriceTrackingPromoCommands;

// Item containing the configurations for the Price Tracking Promo Module view.
@interface PriceTrackingPromoItem
    : StandaloneModuleViewConfig <StandaloneModuleViewTapDelegate>

// Command handler for user actions.
@property(nonatomic, weak) id<PriceTrackingPromoCommands>
    priceTrackingPromoHandler;

// Serialized image data for the most recent price tracked product image.
@property(nonatomic, strong) NSData* productImageData;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_UI_PRICE_TRACKING_PROMO_ITEM_H_

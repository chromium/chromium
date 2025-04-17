// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_DATA_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_DATA_H_

#import <UIKit/UIKit.h>

#import <optional>
#import <string>

#import "components/commerce/core/commerce_types.h"

class GURL;

// Represents a price drop for a shopping URL -
// the current price and previous price.
struct PriceDrop {
  // Formatted current price
  NSString* current_price;
  // Formatted previous price
  NSString* previous_price;
};

enum class ShopCardItemType {
  kUnknown = 0,
  kPriceDropForTrackedProducts,
  kReviews,
  kPriceDropOnTab,
  kPriceTrackableProductOnTab,
};

class GURL;

// Data object for ShopCard, including card type and card-specific data.
@interface ShopCardData : NSObject

// Type of ShopCard.
@property(nonatomic, readwrite) ShopCardItemType shopCardItemType;

// Price Drop if it exists for the URL corresponding to the ShopCard.
@property(nonatomic, assign) std::optional<PriceDrop> priceDrop;

// Describes contents of the ShopCard for accessibility.
@property(nonatomic, copy) NSString* accessibilityString;

// Title of Product.
@property(nonatomic, copy) NSString* productTitle;

// Merchant url for product.
@property(nonatomic, assign) const GURL& productURL;

// Product favicon image.
@property(nonatomic, strong) UIImage* faviconImage;

// Product image.
@property(nonatomic, strong) NSData* productImage;

// URL for product image.
@property(nonatomic, assign) std::optional<std::string> productImageURL;

// ProductInfo from OptimizationGuide on demand API. Long term ShoppingService
// will handle price tracking of synced Tabs TODO(crbug.com/410811501). However,
// short term ProductInfo will be acquired from OptimizationGuide on demand API
// and passed to price tracking API.
@property(nonatomic, assign) std::optional<commerce::ProductInfo> productInfo;

// Current price of the product the ShopCard displays.
@property(nonatomic, copy) NSString* currentPrice;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_DATA_H_

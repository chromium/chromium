// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_DATA_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_DATA_H_

#import <UIKit/UIKit.h>

#import <optional>

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

// Data object for ShopCard, including card type and card-specific data.
@interface ShopCardData : NSObject

// Type of ShopCard.
@property(nonatomic, readwrite) ShopCardItemType shopCardItemType;

// Price Drop if it exists for the URL corresponding to the ShopCard.
@property(nonatomic, assign) std::optional<PriceDrop> priceDrop;

// Describes contents of the ShopCard for accessibility.
@property(nonatomic, copy) NSString* accessibilityString;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_DATA_H_

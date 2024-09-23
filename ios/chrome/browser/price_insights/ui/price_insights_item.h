// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_ITEM_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_ITEM_H_

#import <UIKit/UIKit.h>

#include <string>

class GURL;

namespace commerce {
enum class PriceBucket;
}

// Base object for Price Insights data. This will be used to pass the data to
// the UICollectionViewCell.
@interface PriceInsightsItem : NSObject

// The product title.
@property(nonatomic, copy) NSString* title;
// The product variants.
@property(nonatomic, copy) NSString* variants;
// The product price currency.
@property(nonatomic, assign) std::string currency;
// The product country code.
@property(nonatomic, assign) std::string country;
// The price history.
@property(nonatomic, copy) NSDictionary* priceHistory;
// The product buying options URL.
@property(nonatomic, assign) const GURL& buyingOptionsURL;
// Whether or not the price can be tracked.
@property(nonatomic, assign) BOOL canPriceTrack;
// Whether or not the price is already being tracked.
@property(nonatomic, assign) BOOL isPriceTracked;
// The product URL.
@property(nonatomic, assign) const GURL& productURL;
// The product cluster id.
@property(nonatomic, assign) uint64_t clusterId;
// The product current price bucket.
@property(nonatomic, assign) commerce::PriceBucket priceBucket;
@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_ITEM_H_

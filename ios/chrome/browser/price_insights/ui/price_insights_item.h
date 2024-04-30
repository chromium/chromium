// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_ITEM_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_ITEM_H_

#import <UIKit/UIKit.h>

class GURL;

// Base object for Price Insights data. This will be used to pass the data to
// the UICollectionViewCell.
@interface PriceInsightsItem : NSObject

// The product title.
@property(nonatomic, copy) NSString* title;
// The product variants.
@property(nonatomic, copy) NSString* variants;
// The product typically low price.
@property(nonatomic, copy) NSString* lowPrice;
// The product typically high price.
@property(nonatomic, copy) NSString* highPrice;
// The product price currency.
@property(nonatomic, copy) NSString* currency;
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

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_ITEM_H_

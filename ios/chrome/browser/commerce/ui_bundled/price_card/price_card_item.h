// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_ITEM_H_
#define IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_ITEM_H_

#import <Foundation/NSString.h>

// Model object representing price information for a shopping website.
@interface PriceCardItem : NSObject

// Create a price card item with `price`, and `previous price`.
- (instancetype)initWithPrice:(NSString*)price
                previousPrice:(NSString*)previousPrice
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, copy) NSString* price;
@property(nonatomic, copy) NSString* previousPrice;
@end

#endif  // IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_ITEM_H_

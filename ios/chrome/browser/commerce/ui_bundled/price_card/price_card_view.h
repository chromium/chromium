// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_VIEW_H_
#define IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface PriceCardView : UIView
// Sets the price drop and makes the PriceCardView visible
- (void)setPriceDrop:(NSString*)currentPrice
       previousPrice:(NSString*)previousPrice;
@end

#endif  // IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_VIEW_H_

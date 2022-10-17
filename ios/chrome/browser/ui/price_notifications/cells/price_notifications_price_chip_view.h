// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_PRICE_CHIP_VIEW_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_PRICE_CHIP_VIEW_H_

#import <UIKit/UIKit.h>

// A UIView that displays either the item's current price or a side-by-side
// comparison of the item's current price to the price at which the user began
// price tracking.
@interface PriceNotificationsPriceChipView : UIView

// Sets the price drop and displays the price comparison UI.
- (void)setPriceDrop:(NSString*)currentPrice
       previousPrice:(NSString*)previousPrice;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_PRICE_CHIP_VIEW_H_
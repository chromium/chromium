// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_CELLS_PRICE_NOTIFICATIONS_PRICE_CHIP_VIEW_H_
#define IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_CELLS_PRICE_NOTIFICATIONS_PRICE_CHIP_VIEW_H_

#import <UIKit/UIKit.h>

// A UIView that displays either the item's current price or a side-by-side
// comparison of the item's current price to the price at which the user began
// price tracking. Property updates will not result in updates to the views
// appearance once the receiver is in the view hierarchy. The intention is
// these properties are set one - the view is not dynamic.
@interface PriceNotificationsPriceChipView : UIView

// Sets the price drop and displays the price comparison UI.
- (void)setPriceDrop:(NSString*)currentPrice
       previousPrice:(NSString*)previousPrice;

// Changing the following values after the receiver is in the view hierarchy
// does not cause the view to update with new values.

// The font used for the current price. The default is UIFontTextStyleFootnote
// with weight UIFontWeightBold.
@property(nonatomic, strong) UIFont* currentPriceFont;

// The font used for the previous price. The default is UIFontTextStyleFootnote.
@property(nonatomic, strong) UIFont* previousPriceFont;

// Whether the previous price has a line through it (YES) or not (NO). The
// default is NO.
@property(nonatomic, assign) BOOL strikeoutPreviousPrice;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_CELLS_PRICE_NOTIFICATIONS_PRICE_CHIP_VIEW_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_PRICE_TRACKING_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_PRICE_TRACKING_VIEW_H_

#import <UIKit/UIKit.h>

@protocol TabResumptionCommands;
@class TabResumptionItem;

// Tab resumption variation which enables a price trackable URL to be
// price tracked.
@interface ShopCardPriceTrackingView : UIView

// Initialize a ShopCardPriceTrackingView with the given `item`.
- (instancetype)initWithItem:(TabResumptionItem*)item;

// The handler that receives ShopCardPriceTrackingView's events.
@property(nonatomic, weak) id<TabResumptionCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_PRICE_TRACKING_VIEW_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_CELL_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"

// UICollectionViewCell that contains data for Price Insights.
@interface PriceInsightsCell : UICollectionViewCell

// Configures the UICollectionViewCell with `PriceInsightsitem`.
- (void)configureWithItem:(PriceInsightsItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_CELL_H_

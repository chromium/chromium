// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MUTATOR_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MUTATOR_H_

#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"

// Protocol to communicate price insights actions to the mediator.
@protocol PriceInsightsMutator <NSObject>

// Begins price tracking the `item`.
- (void)priceInsightsTrackItem:(PriceInsightsItem*)item;

// Stops price tracking the `item`.
- (void)priceInsightsStopTrackingItem:(PriceInsightsItem*)item;

// Navigates the current WebState to `item`'s webpage.
- (void)priceInsightsNavigateToWebpageForItem:(PriceInsightsItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MUTATOR_H_

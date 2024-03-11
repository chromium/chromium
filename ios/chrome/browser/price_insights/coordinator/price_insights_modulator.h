// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MODULATOR_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MODULATOR_H_

#import <UIKit/UIKit.h>

// A coordinator-like object for Price Insights integrated into the contextual
// panel as an info block.
@interface PriceInsightsModulator : NSObject

// Starts the modulator.
- (void)start;

// Stops the modulator.
- (void)stop;

// Return the Price Insights UICollectionViewCell registration.
- (UICollectionViewCellRegistration*)cellRegistration;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MODULATOR_H_

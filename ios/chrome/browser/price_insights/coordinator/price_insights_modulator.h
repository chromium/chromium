// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MODULATOR_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MODULATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/price_insights/coordinator/price_insights_consumer.h"

class Browser;

// A coordinator-like object for Price Insights integrated into the contextual
// panel as an info block.
@interface PriceInsightsModulator : NSObject <PriceInsightsConsumer>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Starts the modulator.
- (void)start;

// Stops the modulator.
- (void)stop;

// Return the Price Insights UICollectionViewCell registration.
- (UICollectionViewCellRegistration*)cellRegistration;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_MODULATOR_H_

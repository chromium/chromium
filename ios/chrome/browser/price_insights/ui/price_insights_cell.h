// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_CELL_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/contextual_panel/ui/panel_item_collection_view_cell.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"

@protocol PriceInsightsMutator;

// UICollectionViewCell that contains data for Price Insights.
@interface PriceInsightsCell : PanelItemCollectionViewCell

// Contextual panel view controller.
@property(nonatomic, weak) UIViewController* viewController;

// Mutator for Price Tracking related actions e.g price tracking event
// subscription.
@property(nonatomic, weak) id<PriceInsightsMutator> mutator;

// PriceInsightsItem object displayed in the cell.
@property(nonatomic, readonly) PriceInsightsItem* priceInsightsItem;

// Configures the UICollectionViewCell with `PriceInsightsitem`.
- (void)configureWithItem:(PriceInsightsItem*)item;

// Updates the item tracking status in the cell.
- (void)updateTrackStatus:(BOOL)isTracking;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_UI_PRICE_INSIGHTS_CELL_H_

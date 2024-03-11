// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/price_insights_modulator.h"

#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"

@implementation PriceInsightsModulator

#pragma mark - Public

- (void)start {
}

- (void)stop {
}

- (UICollectionViewCellRegistration*)cellRegistration {
  __weak __typeof(self) weakSelf = self;
  auto handler =
      ^(PriceInsightsCell* cell, NSIndexPath* indexPath, id identifier) {
        [weakSelf configureCell:cell];
      };
  return [UICollectionViewCellRegistration
      registrationWithCellClass:[PriceInsightsCell class]
           configurationHandler:handler];
}

#pragma mark - private

// Cell configuration handler helper.
- (void)configureCell:(PriceInsightsCell*)cell {
  PriceInsightsItem* item = [[PriceInsightsItem alloc] init];
  [cell configureWithConfig:item];
}

@end

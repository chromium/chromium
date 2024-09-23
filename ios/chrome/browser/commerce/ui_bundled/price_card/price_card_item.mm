// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_item.h"

@implementation PriceCardItem

- (instancetype)initWithPrice:(NSString*)price
                previousPrice:(NSString*)previousPrice {
  if ((self = [super init])) {
    _price = price;
    _previousPrice = previousPrice;
  }
  return self;
}

@end

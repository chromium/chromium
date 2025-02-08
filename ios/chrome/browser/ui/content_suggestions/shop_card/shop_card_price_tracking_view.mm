// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_price_tracking_view.h"

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"

@implementation ShopCardPriceTrackingView {
  // Item used to configure the view.
  TabResumptionItem* _item;
}

- (instancetype)initWithItem:(TabResumptionItem*)item {
  self = [super init];
  if (self) {
    _item = item;
  }
  return self;
}

@end

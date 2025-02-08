// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_view.h"

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_data.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_item.h"

@implementation ShopCardModuleView {
}

- (instancetype)initWithFrame {
  self = [super initWithFrame:CGRectZero];
  return self;
}

- (void)configureView:(ShopCardItem*)config {
  if (config.shopCardData.shopCardItemType ==
      ShopCardItemType::kPriceDropForTrackedProducts) {
    // TODO: crbug.com/394638800 - render correct view when data available
  } else if (config.shopCardData.shopCardItemType ==
             ShopCardItemType::kReviews) {
    // TODO: crbug.com/394638800 - render correct view when data available
  }
}

@end

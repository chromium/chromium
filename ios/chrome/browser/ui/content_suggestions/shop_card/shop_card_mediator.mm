// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_data.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_item.h"

@implementation ShopCardMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  ShopCardItem* _shopCardItem;
}

- (instancetype)initWithShoppingService:
    (commerce::ShoppingService*)shoppingService {
  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
  }
  return self;
}

- (void)disconnect {
  _shoppingService = nil;
}

- (void)reset {
  _shopCardItem = nil;
}

- (void)setDelegate:(id<ShopCardMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLatestShopCardItem];
  }
}

- (void)fetchLatestShopCardItem {
  // Populate the item if it is not already initialized.
  _shopCardItem = [[ShopCardItem alloc] init];
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
    _shopCardItem.shouldShowSeeMore = YES;
  }

  _shopCardItem.shopCardData = [[ShopCardData alloc] init];

  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
    _shopCardItem.shopCardData.shopCardItemType =
        ShopCardItemType::kPriceDropForTrackedProducts;
  } else if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm2) {
    _shopCardItem.shopCardData.shopCardItemType = ShopCardItemType::kReviews;
  }
}

- (ShopCardItem*)shopCardItemToShow {
  return _shopCardItem;
}

#pragma mark - Public
- (void)disableModule {
}

#pragma mark - ShopCardMediatorDelegate

- (void)removeShopCard {
  [self disableModule];
}

#pragma mark - Testing category methods
- (commerce::ShoppingService*)shoppingServiceForTesting {
  return self->_shoppingService;
}

- (void)setShopCardItemForTesting:(ShopCardItem*)item {
  self->_shopCardItem = item;
}

- (ShopCardItem*)shopCardItemForTesting {
  return self->_shopCardItem;
}

@end

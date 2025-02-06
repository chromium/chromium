// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_mediator.h"

#import "base/memory/raw_ptr.h"
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
  // This is a placeholder and may get replaced when we fetch actual data.
  if (self->_shopCardItem.shopCardData) {
    return;
  }
  _shopCardItem = [[ShopCardItem alloc] init];
  _shopCardItem.shopCardData = [[ShopCardData alloc] init];
  // TODO: crbug.com/394638800 - set this to the correct type based on
  // experiment.
  _shopCardItem.shopCardData.shopCardItemType = ShopCardItemType::kUnknown;
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

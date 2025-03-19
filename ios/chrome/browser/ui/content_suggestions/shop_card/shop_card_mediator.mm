// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_action_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_data.h"
#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_item.h"

@interface ShopCardMediator () <PrefObserverDelegate>
@end

@implementation ShopCardMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  ShopCardItem* _shopCardItem;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  raw_ptr<PrefService> _prefService;
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithShoppingService:
                    (commerce::ShoppingService*)shoppingService
                            prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
    _prefService = prefService;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled,
        &_prefChangeRegistrar);
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
  if (!_prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled)) {
    return;
  }
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

- (void)openShopCardItem:(ShopCardItem*)item {
  [self.shopCardActionDelegate openURL:item.shopCardData.productURL];
}

#pragma mark - ShopCardMediatorDelegate

- (void)removeShopCard {
  [self disableModule];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName ==
      prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled) {
    if (_prefService->GetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled)) {
      // TODO(crbug.com/404564187) Fetch ShopCardData if ShopCardData
      // is nil, then insert the card.
      [self.delegate insertShopCard];
    } else {
      [self.delegate removeShopCard];
    }
  }
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

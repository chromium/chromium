// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"

@implementation PriceTrackingPromoMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  PriceTrackingPromoItem* _priceTrackingPromoItem;
}

- (instancetype)initWithShoppingService:
    (commerce::ShoppingService*)shoppingService {
  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
    // _priceTrackingPromoItem will ultimately be filled with data
    // fetched via ShoppingService. However, for now use a blank object
    // to hold the magic stack integration together and enable magic
    // stack card to be build out with static assets. This will
    // be removed when TODO(crbug.com/361106168) is implemented.
    _priceTrackingPromoItem = [[PriceTrackingPromoItem alloc] init];
    _priceTrackingPromoItem.commandHandler = self;
  }
  return self;
}

- (void)disconnect {
  _shoppingService = nil;
}

- (void)reset {
  _priceTrackingPromoItem = nil;
}

- (void)fetchLatestSubscription {
  // TODO(crbug.com/361405189) fetch latest subscription and
  // convert to PriceTrackingPromoItem.
}

- (PriceTrackingPromoItem*)priceTrackingPromoItemToShow {
  return _priceTrackingPromoItem;
}

#pragma mark - Public

- (void)disableModule {
  // TODO(crbug.com/361404422) implement response to
  // user choosing to disable module.
}

#pragma mark - PriceTrackingPromoCommands

- (void)allowPriceTrackingNotifications {
  [PushNotificationUtil requestPushNotificationPermission:^(
                            BOOL granted, BOOL promptShown, NSError* error) {
    [self.delegate removePriceTrackingPromo];
  }];
  // TODO(crbug.com/361107178) implement opt in flow B
  // TODO(crbug.com/361107641) implement opt in flow C
}

#pragma mark - Testing category methods

- (commerce::ShoppingService*)shoppingServiceForTesting {
  return self->_shoppingService;
}

- (PriceTrackingPromoItem*)priceTrackingPromoItemForTesting {
  return self->_priceTrackingPromoItem;
}

@end

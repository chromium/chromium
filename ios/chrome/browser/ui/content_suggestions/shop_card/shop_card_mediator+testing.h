// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_mediator.h"

@class ShopCardItem;

namespace commerce {
class ShoppingService;
}  // namespace commerce

// Category for exposing internal state for testing.
@interface ShopCardMediator (ForTesting)

- (commerce::ShoppingService*)shoppingServiceForTesting;

- (ShopCardItem*)shopCardItemForTesting;

- (void)setShopCardItemForTesting:(ShopCardItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_MEDIATOR_TESTING_H_

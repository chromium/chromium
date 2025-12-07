// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/coordinator/shop_card_mediator.h"

@class ShopCardItem;

namespace commerce {
class ShoppingService;
}  // namespace commerce

// Category for exposing internal state for testing.
@interface ShopCardMediator (ForTesting)

- (commerce::ShoppingService*)shoppingServiceForTesting;

- (ShopCardItem*)shopCardItemForTesting;

- (void)setShopCardItemForTesting:(ShopCardItem*)item;

- (void)logImpressionForItemForTesting:(ShopCardItem*)item;

- (BOOL)hasReachedImpressionLimitForTesting:(const GURL&)url;

- (void)logEngagementForItemForTesting:(ShopCardItem*)item;

- (BOOL)hasBeenOpenedForTesting:(const GURL&)url;

- (void)onUrlUntrackedForTesting:(GURL)url;

- (void)fetchPriceTrackedBookmarksForTesting;

- (void)fetchPriceTrackedBookmarksIfApplicableForTesting;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_TESTING_H_

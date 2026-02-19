// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/content_suggestions/shop_card/coordinator/shop_card_mediator.h"

@class ShopCardConfig;

namespace commerce {
class ShoppingService;
}  // namespace commerce

// Category for exposing internal state for testing.
@interface ShopCardMediator (ForTesting)

- (commerce::ShoppingService*)shoppingServiceForTesting;

- (ShopCardConfig*)shopCardConfigForTesting;

- (void)setShopCardConfigForTesting:(ShopCardConfig*)config;

- (void)logImpressionForItemForTesting:(ShopCardConfig*)config;

- (BOOL)hasReachedImpressionLimitForTesting:(const GURL&)url;

- (void)logEngagementForItemForTesting:(ShopCardConfig*)config;

- (BOOL)hasBeenOpenedForTesting:(const GURL&)url;

- (void)onUrlUntrackedForTesting:(GURL)url;

- (void)fetchPriceTrackedBookmarksForTesting;

- (void)fetchPriceTrackedBookmarksIfApplicableForTesting;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_TESTING_H_

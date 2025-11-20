// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_DELEGATE_H_

// Delegate to communicate events back to owner of ShopCardMediator.
@protocol ShopCardMediatorDelegate

// Remove ShopCard from the magic stack.
- (void)removeShopCard;

// Add ShopCard to the magic stack.
- (void)insertShopCard;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_MEDIATOR_DELEGATE_H_

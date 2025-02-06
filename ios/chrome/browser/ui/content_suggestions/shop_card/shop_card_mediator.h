// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_commands.h"

namespace commerce {
class ShoppingService;
}

@class ShopCardItem;
@class ShopCardData;

// Delegate to communicate events back to owner of ShopCardMediator.
@protocol ShopCardMediatorDelegate
- (void)removeShopCard;
@end

@interface ShopCardMediator : NSObject <ShopCardCommands>

// Default initializer.
- (instancetype)initWithShoppingService:
    (commerce::ShoppingService*)shoppingService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

- (void)reset;

- (void)disableModule;

- (ShopCardItem*)shopCardItemToShow;

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<ShopCardMediatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_MEDIATOR_H_

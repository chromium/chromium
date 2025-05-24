// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_ITEM_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module.h"

@protocol ShopCardCommands;
@class ShopCardData;
@protocol ShopCardFaviconConsumerSource;

// Item containing the configurations for the Shopcard view.
@interface ShopCardItem : MagicStackModule

// Command handler for user actions.
@property(nonatomic, weak) id<ShopCardCommands> commandHandler;

// Shopping data including the card type, and card-specific data.
@property(nonatomic, strong) ShopCardData* shopCardData;

// Consumer source (e.g. mediator) that receives the favicon update.
@property(nonatomic, strong) id<ShopCardFaviconConsumerSource>
    shopCardFaviconConsumerSource;
@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_ITEM_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_ITEM_H_

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

@protocol ShopCardCommands;
@class ShopCardData;

// Item containing the configurations for the Shopcard view.
@interface ShopCardItem : MagicStackModule

// Command handler for user actions.
@property(nonatomic, weak) id<ShopCardCommands> commandHandler;

// Shopping data including the card type, and card-specific data.
@property(nonatomic, strong) ShopCardData* shopCardData;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_ITEM_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_UI_SHOP_CARD_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_UI_SHOP_CARD_CONFIG_H_

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

@protocol ShopCardCommands;
@class ShopCardData;

// Item containing the configurations for the Shopcard view.
@interface ShopCardConfig : MagicStackModule

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// Shopping data including the card type, and card-specific data.
@property(nonatomic, strong) ShopCardData* shopCardData;

// Command handler for user actions.
@property(nonatomic, weak) id<ShopCardCommands> shopCardHandler;
// LINT.ThenChange(shop_card_config.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_UI_SHOP_CARD_CONFIG_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_COMMANDS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_COMMANDS_H_

@class ShopCardItem;

// Command protocol for events of ShopCard module.
@protocol ShopCardCommands

// Opens the displayed shop card item.
- (void)openShopCardItem:(ShopCardItem*)item;

@end
#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_COMMANDS_H_

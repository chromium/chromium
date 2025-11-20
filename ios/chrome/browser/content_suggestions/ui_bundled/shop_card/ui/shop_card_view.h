// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_favicon_consumer.h"

@protocol ShopCardCommands;
@protocol ShopCardFaviconConsumer;
@class ShopCardItem;

// View for the Shop Card module.
@interface ShopCardModuleView : UIView <ShopCardFaviconConsumer>

- (instancetype)initWithFrame;

// Configures this view with `config`.
- (void)configureView:(ShopCardItem*)config;

// Command handler for user events.
@property(nonatomic, weak) id<ShopCardCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_VIEW_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_UI_SHOP_CARD_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_UI_SHOP_CARD_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ShopCardCommands;
@class ShopCardConfig;

// View for the Shop Card module.
@interface ShopCardModuleView : UIView

// Command handler for user events.
@property(nonatomic, weak) id<ShopCardCommands> commandHandler;

// Configures this view with `config`.
- (void)configureView:(ShopCardConfig*)config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHOP_CARD_UI_SHOP_CARD_VIEW_H_

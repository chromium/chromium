// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ShopCardCommands;
@class ShopCardItem;

@interface ShopCardModuleView : UIView

- (instancetype)initWithFrame;

// Configures this view with `config`.
- (void)configureView:(ShopCardItem*)config;

// Command handler for user events.
@property(nonatomic, weak) id<ShopCardCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SHOP_CARD_SHOP_CARD_VIEW_H_

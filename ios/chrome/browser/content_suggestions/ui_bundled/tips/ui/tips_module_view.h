// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TIPS_UI_TIPS_MODULE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TIPS_UI_TIPS_MODULE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/ui/tips_magic_stack_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_updating.h"

@protocol MagicStackModuleContentViewDelegate;
@protocol TipsModuleAudience;
@class TipsModuleState;

// A view displaying the Tips module in the Magic Stack.
@interface TipsModuleView
    : UIView <NewTabPageColorUpdating, TipsMagicStackConsumer>

// Initializes the `TipsModuleView` with `state`.
- (instancetype)initWithState:(TipsModuleState*)state;

// The object that should handle user events.
@property(nonatomic, weak) id<TipsModuleAudience> audience;

// The delegate for handling content view events.
@property(nonatomic, weak) id<MagicStackModuleContentViewDelegate>
    contentViewDelegate;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TIPS_UI_TIPS_MODULE_VIEW_H_

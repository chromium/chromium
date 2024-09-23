// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_CONFIG_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_CONFIG_H_

#import <UIKit/UIKit.h>

@protocol ShortcutsCommands;
@protocol ShortcutsConsumerSource;
@class ContentSuggestionsMostVisitedActionItem;

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

// Config object for the Shortcuts module.
@interface ShortcutsConfig : MagicStackModule

// List of Shortcuts to show in module.
@property(nonatomic, strong)
    NSArray<ContentSuggestionsMostVisitedActionItem*>* shortcutItems;

// Shortcuts model.
@property(nonatomic, weak) id<ShortcutsConsumerSource> consumerSource;

// Command handler for user actions.
@property(nonatomic, weak) id<ShortcutsCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_CONFIG_H_

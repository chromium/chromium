// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONFIG_H_

#import <UIKit/UIKit.h>

@protocol ShortcutsCommands;
@protocol ShortcutsConsumerSource;
@class ShortcutsActionItem;

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module.h"

// Config object for the Shortcuts module.
@interface ShortcutsConfig : MagicStackModule

// List of Shortcuts to show in module.
@property(nonatomic, strong) NSArray<ShortcutsActionItem*>* shortcutItems;

// Shortcuts model.
@property(nonatomic, weak) id<ShortcutsConsumerSource> consumerSource;

// Command handler for user actions.
@property(nonatomic, weak) id<ShortcutsCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONFIG_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHORTCUTS_UI_SHORTCUTS_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHORTCUTS_UI_SHORTCUTS_CONFIG_H_

#import <UIKit/UIKit.h>

@protocol ShortcutsCommands;
@class ShortcutsActionItem;

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

// Config object for the Shortcuts module.
@interface ShortcutsConfig : MagicStackModule

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// List of Shortcuts to show in module.
@property(nonatomic, copy) NSArray<ShortcutsActionItem*>* shortcutItems;

// Command handler for user actions.
@property(nonatomic, weak) id<ShortcutsCommands> commandHandler;
// LINT.ThenChange(shortcuts_config.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SHORTCUTS_UI_SHORTCUTS_CONFIG_H_

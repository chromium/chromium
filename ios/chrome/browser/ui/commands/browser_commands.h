// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"

@class ReadingListAddCommand;

// Protocol for commands that will generally be handled by the "current tab",
// which in practice is the BrowserViewController instance displaying the tab.
@protocol BrowserCommands <NSObject>

// Closes the current tab.
// TODO(crbug.com/1272498): Refactor this command away; call sites should close
// via the WebStateList.
- (void)closeCurrentTab;

// Adds a page to the reading list using data in `command`.
// TODO(crbug.com/1272540): Remove this command.
- (void)addToReadingList:(ReadingListAddCommand*)command;

// Preloads voice search on the current BVC.
- (void)preloadVoiceSearch;

// Prepares the browser to display a popup menu.
- (void)prepareForPopupMenuPresentation:(PopupMenuCommandType)type;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_

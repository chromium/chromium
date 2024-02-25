// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_COMMANDS_H_

#import <UIKit/UIKit.h>

// Command protocol for events for the Shortcuts module.
@protocol ShortcutsCommands

// Indicates to the receiver that a Shortcuts tile `sender` was tapped.
- (void)shortcutsTapped:(UIGestureRecognizer*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_COMMANDS_H_

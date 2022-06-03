// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_HELP_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_HELP_COMMANDS_H_

// Commands to control the display of in-product help UI ("bubbles").
@protocol HelpCommands <NSObject>

// Shows a relevant help bubble, if any.
- (void)showHelpBubbleIfEligible;

// Shows a relevant help bubble for long-press state, if any.
- (void)showLongPressHelpBubbleIfEligible;

// Dismisses all bubbles.
- (void)hideAllHelpBubbles;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_HELP_COMMANDS_H_

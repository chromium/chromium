// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_

// Commands to control the display of in-product help UI ("bubbles").
@protocol HelpCommands <NSObject>

// Dismisses all bubbles.
- (void)hideAllHelpBubbles;

// Shows a help bubble for the share button, if eligible.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentShareButtonHelpBubbleIfEligible;

// Logs the event in feature engagement tracker and, if eligible, shows a
// gesture-based in-product help for the pull-to-refresh feature once the active
// tab has finished loading a URL.
//
// Should be invoked when the user performs a multi-gesture page refresh. The
// eligibility can depend on the UI hierarchy at the moment, the configuration
// and the display history of the IPH, etc.
// TODO(crbug.com/1467873): Refactor out of this protocol; commands in here are
// not supposed to behave asynchronously.
- (void)notifyMultiGestureRefreshAndShowHelpBubbleIfEligible;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_

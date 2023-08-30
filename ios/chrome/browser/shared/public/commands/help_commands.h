// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_

// Commands to control the display of in-product help UI ("bubbles").
@protocol HelpCommands <NSObject>

// Dismisses all bubbles.
- (void)hideAllHelpBubbles;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_HELP_COMMANDS_H_

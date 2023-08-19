// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_

// Commands for tab strip changes.
@protocol TabStripCommands

// Set the `iphHighlighted` state for the new tab button on the tab strip.
- (void)setNewTabButtonOnTabStripIPHHighlighted:(BOOL)IPHHighlighted;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_

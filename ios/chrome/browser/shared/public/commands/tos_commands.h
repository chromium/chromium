// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOS_COMMANDS_H_

// Commands related to the Terms of Service (ToS) UI use in welcome screen from
// the first run experience. It displays the ToS in modal view with a top
// navigation bar with a button which should dismiss the page. Those commands
// should manage the ToS coordinator.
@protocol TOSCommands

// Shows the ToS page.
- (void)showTOSPage;

// Closes the ToS page.
- (void)closeTOSPage;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOS_COMMANDS_H_

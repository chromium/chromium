// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GROUP_CONFIRMATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GROUP_CONFIRMATION_COMMANDS_H_

// Protocol used to send commands to the tab group confirmation dialog.
@protocol TabGroupConfirmationCommands

// Dismisses the confirmation dialog if it's displayed.
- (void)dismissTabGroupConfirmation;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GROUP_CONFIRMATION_COMMANDS_H_

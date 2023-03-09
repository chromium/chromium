// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POLICY_CHANGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POLICY_CHANGE_COMMANDS_H_

@protocol PolicyChangeCommands <NSObject>

// Command to show a prompt to warn the user that they have been signed
// out due to a policy change (force sign out). The prompt is shown immediately
// and stays on-screen until the user dismisses it.
- (void)showForceSignedOutPrompt;

// Command to show a prompt to warn the user that sync has been disabled due to
// a policy change. The prompt is shown immediately and stays until the user
// dismisses it.
- (void)showSyncDisabledPrompt;

// Command to show a prompt to warn the user that they have been signed
// out due to a policy change (restrict account to patterns). The prompt is
// shown immediately and stays on-screen until the user dismisses it.
- (void)showRestrictAccountSignedOutPrompt;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_POLICY_CHANGE_COMMANDS_H_

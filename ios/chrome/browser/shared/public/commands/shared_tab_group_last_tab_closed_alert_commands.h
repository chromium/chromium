// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARED_TAB_GROUP_LAST_TAB_CLOSED_ALERT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARED_TAB_GROUP_LAST_TAB_CLOSED_ALERT_COMMANDS_H_

@class SharedTabGroupLastTabAlertCommand;

// Protocol to display the last tab in a shared group alert.
@protocol SharedTabGroupLastTabAlertCommands

// Starts the last tab in shared group alert.
- (void)showLastTabInSharedGroupAlert:
    (SharedTabGroupLastTabAlertCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARED_TAB_GROUP_LAST_TAB_CLOSED_ALERT_COMMANDS_H_

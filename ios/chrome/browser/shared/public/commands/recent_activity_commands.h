// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_RECENT_ACTIVITY_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_RECENT_ACTIVITY_COMMANDS_H_

class TabGroup;

// Command protocol for the Recent Activity.
@protocol RecentActivityCommands

// Dismisses the recent activity and exit the tab grid.
- (void)dismissViewAndExitTabGrid;

// Dismisses the recent activity and show the manage screen for `group`.
- (void)showManageScreenForGroup:(const TabGroup*)group;

// Dismisses the recent activity and show the edit screen for `group`.
- (void)showTabGroupEditForGroup:(const TabGroup*)group;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_RECENT_ACTIVITY_COMMANDS_H_

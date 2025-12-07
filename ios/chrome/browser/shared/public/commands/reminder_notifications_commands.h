// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_REMINDER_NOTIFICATIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_REMINDER_NOTIFICATIONS_COMMANDS_H_

#import <Foundation/Foundation.h>

// Entry points for opening the reminder notifications 'Set Tab Reminder' UI.
enum class SetTabReminderEntryPoint {
  kOverflowMenu = 0,            // Opened by overflow menu action on current tab
  kTabGridLongPress = 1,        // Opened by long press in tab grid
  kBookmarksSwipeAction = 2,    // Opened from swipe actions in Bookmarks
  kReadingListSwipeAction = 3,  // Opened from swipe actions in Reading List
  kPinnedTabLongPress = 4       // Opened by long press on pinned tab
};

// Commands for interacting with reminder notifications UI.
@protocol ReminderNotificationsCommands

// Shows the Set Tab Reminder UI.
- (void)showSetTabReminderUI:(SetTabReminderEntryPoint)entryPoint;

// Dismisses the Set Tab Reminder UI.
- (void)dismissSetTabReminderUI;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_REMINDER_NOTIFICATIONS_COMMANDS_H_

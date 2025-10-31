// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SYNCED_SET_UP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SYNCED_SET_UP_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands related to Synced Set Up.
@protocol SyncedSetUpCommands

// Shows the Synced Set Up UI.
- (void)showSyncedSetUp;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SYNCED_SET_UP_COMMANDS_H_

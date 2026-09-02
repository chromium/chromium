// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class SyncedSetUpCoordinator;

// Delegate for events from the `SyncedSetUpCoordinator`.
@protocol SyncedSetUpCoordinatorDelegate <NSObject>

// Called when the Synced Set Up flow finishes.
- (void)syncedSetUpCoordinatorDidFinish:(SyncedSetUpCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_DELEGATE_H_

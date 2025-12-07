// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SyncedSetUpCoordinatorDelegate;

// Coordinator that orchestrates the Synced Set Up experience.
@interface SyncedSetUpCoordinator : ChromeCoordinator

// The delegate that receives events from this coordinator.
@property(nonatomic, weak) id<SyncedSetUpCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_H_

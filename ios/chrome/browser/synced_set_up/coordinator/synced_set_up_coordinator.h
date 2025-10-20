// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class AppStartupParameters;
@protocol SyncedSetUpCoordinatorDelegate;

// Coordinator that orchestrates the Synced Set Up experience.
@interface SyncedSetUpCoordinator : ChromeCoordinator

// The delegate that receives events from this coordinator.
@property(nonatomic, weak) id<SyncedSetUpCoordinatorDelegate> delegate;

// Initializes the `SyncedSetUpCoordinator`, using the provided
// `viewController`, `browser`, and `startupParameters` to adapt the Synced Set
// Up flow to the application's launch context.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                         startupParameters:
                             (AppStartupParameters*)startupParameters
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_COORDINATOR_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

@class HistorySyncCoordinator;

// Delegate for the history sync coordinator.
@protocol HistorySyncCoordinatorDelegate <NSObject>

// Called once the dialog can be closed.
- (void)closeHistorySyncCoordinator:
            (HistorySyncCoordinator*)historySyncCoordinator
                     declinedByUser:(BOOL)declined;

@end

// Coordinator for history sync view. The current implementation supports only
// showing the view in a navigation controller.
@interface HistorySyncCoordinator : InterruptibleChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initiates a HistorySyncCoordinator with `navigationController`,
// `browser` and `delegate`.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:
                                            (id<HistorySyncCoordinatorDelegate>)
                                                delegate
                                        firstRun:(BOOL)firstRun
                                   showUserEmail:(BOOL)showUserEmail
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_

#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

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
    NS_DESIGNATED_INITIALIZER;

// Completion block called once the dialog can be closed.
// `success` if YES, the user is syncing.
@property(nonatomic, copy) signin_ui::CompletionCallback coordinatorCompleted;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_

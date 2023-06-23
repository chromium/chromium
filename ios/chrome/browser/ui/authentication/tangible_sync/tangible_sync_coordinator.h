// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

// Coordinator for tangible sync view. The current implementation supports only
// to show the view in a navigation controller.
@interface TangibleSyncCoordinator : InterruptibleChromeCoordinator

// Completion block called once the dialog can be closed.
@property(nonatomic, copy) ProceduralBlock coordinatorCompleted;

// TODO(crbug.com/1363812): Need to support to present as a modal dialog.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initiates a TangibleSyncCoordinator with `navigationController`,
// `browser` and `delegate`.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        firstRun:(BOOL)firstRun
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_COORDINATOR_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"

// Coordinator to present sync screen.
@interface SyncScreenCoordinator : InterruptibleChromeCoordinator

// Initiates a SyncScreenCoordinator with
// `navigationController` to present the view;
// `browser` to provide the browser;
// `delegate` to handle user action.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:
                                            (id<FirstRunScreenDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_COORDINATOR_H_

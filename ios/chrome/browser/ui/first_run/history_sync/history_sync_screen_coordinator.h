// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_HISTORY_SYNC_HISTORY_SYNC_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_HISTORY_SYNC_HISTORY_SYNC_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

// Coordinator to present the history sync screen.
@interface HistorySyncScreenCoordinator : InterruptibleChromeCoordinator

// Initiates a SyncScreenCoordinator with
// `navigationController` to present the view;
// `browser` to provide the browser;
// `firstRun` to determine whether this is used in the FRE;
// `delegate` to handle user action.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        firstRun:(BOOL)firstRun
                                        delegate:
                                            (id<FirstRunScreenDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_HISTORY_SYNC_HISTORY_SYNC_SCREEN_COORDINATOR_H_

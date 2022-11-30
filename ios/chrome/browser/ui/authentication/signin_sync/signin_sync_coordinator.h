// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_COORDINATOR_H_

#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

@protocol FirstRunScreenDelegate;

// Coordinator for the screen allowing the user to Sign-in and Sync.
@interface SigninSyncCoordinator : InterruptibleChromeCoordinator

// Initiates a SigninSyncCoordinator with `navigationController`,
// `browser` and `delegate`.
// The `delegate` parameter is for handling the transfer between screens.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:
                                            (id<FirstRunScreenDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_COORDINATOR_H_

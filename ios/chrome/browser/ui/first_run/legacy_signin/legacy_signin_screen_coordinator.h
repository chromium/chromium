// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

@protocol FirstRunScreenDelegate;

// Coordinator to present sign-in screen.
// Need to remove this class when crbug.com/1290848 is fixed.
@interface LegacySigninScreenCoordinator : InterruptibleChromeCoordinator

// Initiates a LegacySigninScreenCoordinator with `navigationController`,
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

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_LEGACY_SIGNIN_LEGACY_SIGNIN_SCREEN_COORDINATOR_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/first_run/ui_bundled/interruptible_chrome_coordinator.h"

@protocol FirstRunScreenDelegate;
namespace signin_metrics {
enum class AccessPoint : int;
enum class PromoAction : int;
}  // namespace signin_metrics

// Coordinator to present sign-in screen with FRE consent (optional).
@interface SigninScreenCoordinator : InterruptibleChromeCoordinator

// Initiates a SigninScreenCoordinator with `navigationController`,
// `browser` and `delegate`.
// The `delegate` parameter is for handling the transfer between screens.
// The `accessPoint` and `promoAction` parameters are used for logging.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            delegate:(id<FirstRunScreenDelegate>)delegate
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                         promoAction:(signin_metrics::PromoAction)promoAction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_COORDINATOR_H_

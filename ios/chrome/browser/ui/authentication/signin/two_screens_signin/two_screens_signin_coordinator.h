// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TWO_SCREENS_SIGNIN_TWO_SCREENS_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TWO_SCREENS_SIGNIN_TWO_SCREENS_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

namespace signin_metrics {
enum class AccessPoint : int;
enum class PromoAction : int;
}  // namespace signin_metrics

// Coordinator to present the sign-in and sync first run screens.
@interface TwoScreensSigninCoordinator
    : SigninCoordinator <FirstRunScreenDelegate>

// Initiate the coordinator.
// `browser` used for authentication. It must not be off the record (incognito).
// `screenProvider` helps decide which screen to show. `accessPoint` and
// `promoAction` are used for logging.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TWO_SCREENS_SIGNIN_TWO_SCREENS_SIGNIN_COORDINATOR_H_

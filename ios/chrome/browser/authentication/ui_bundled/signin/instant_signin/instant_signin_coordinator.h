// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

namespace signin_metrics {
enum class AccessPoint;
enum class PromoAction;
}  // namespace signin_metrics

// This sign-in coordinator ensures that the sign-in flow is triggered with
// least tap possible. As a SigninCoordinator, the result is sent to
// SigninCoordinatorCompletionCallback. The sign-in flow is done with the
// following identity, either:
// * `identity` if it’s not nil, or
// * one selected by the identity chooser that gets immediately opened, if the
// device has identities, or
// * otherwise, one obtained through the add account dialog.
@interface InstantSigninCoordinator : SigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                      identity:(id<SystemIdentity>)identity
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction
          continuationProvider:
              (const ChangeProfileContinuationProvider&)continuationProvider
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_COORDINATOR_H_

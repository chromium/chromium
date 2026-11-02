// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

namespace signin_metrics {
enum class AccessPoint : int;
}

// Coordinates various Identity options in Chrome including signing in
// using accounts on the device, opening Incognito, and adding an account.
@interface ConsistencyPromoSigninCoordinator : SigninCoordinator

// Initializes the coordinator.
// * `confirmChangeProfile` is a block called if the successful authentication
//   flow would cause a profile switch, to get the user to decide whether they
//   want this switch to occur or not. Can be nil, which means the user always
//   accepts.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
          confirmChangeProfile:
              (SigninChangeProfileConfirmationBlock)confirmChangeProfile
          prepareChangeProfile:(ProceduralBlock)prepareChangeProfile
          continuationProvider:
              (const ChangeProfileContinuationProvider&)continuationProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_H_

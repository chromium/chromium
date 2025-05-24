// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

namespace signin_metrics {
enum class AccessPoint : int;
}

// Coordinates various Identity options in Chrome including signing in
// using accounts on the device, opening Incognito, and adding an account.
@interface ConsistencyPromoSigninCoordinator : SigninCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
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

+ (instancetype)
    coordinatorWithBaseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
                         contextStyle:(SigninContextStyle)contextStyle
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                 prepareChangeProfile:(ProceduralBlock)prepareChangeProfile
                 continuationProvider:(const ChangeProfileContinuationProvider&)
                                          continuationProvider;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_COORDINATOR_H_

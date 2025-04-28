// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_enums.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

// Coordinates adding an account with different intents:
//  + adding account from the settings
//  + reauthentication
@interface AddAccountSigninCoordinator : SigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

// Designated initializer.
// `viewController` presents the sign-in.
// `accessPoint` is the view where the sign-in button was displayed.
// `promoAction` is promo button used to trigger the sign-in.
// `signinIntent` is the sign-in flow that will be triggered.
// `contextStyle` is used to customize content on screens.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction
                  signinIntent:(AddAccountSigninIntent)signinIntent
          continuationProvider:
              (const ChangeProfileContinuationProvider&)continuationProvider
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_COORDINATOR_H_

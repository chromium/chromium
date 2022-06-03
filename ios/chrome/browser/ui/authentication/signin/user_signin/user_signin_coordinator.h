// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"

@class UserSigninLogger;

// Coordinates the user sign-in with different intents:
//  + user sign-in when triggered from UI (settings, bookmarks...)
//  + first run sign-in
//  + Chrome upgrade sign-in
@interface UserSigninCoordinator : SigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// |viewController| presents the sign-in.
// |identity| is the identity preselected with the sign-in opens.
// |signinIntent| is the intent for the UI displayed in the sign-in flow.
// |logger| is the logger for sign-in flow operations. This should not be nil.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(ChromeIdentity*)identity
                              signinIntent:(UserSigninIntent)signinIntent
                                    logger:(UserSigninLogger*)logger
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer using UINavigationController.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                    signinIntent:(UserSigninIntent)signinIntent
                                          logger:(UserSigninLogger*)logger;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_COORDINATOR_H_

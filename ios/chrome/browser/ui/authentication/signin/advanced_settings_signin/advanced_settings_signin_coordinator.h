// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_COORDINATOR_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

// Coordinates the advanced settings to finish the sign-in flow.
@interface AdvancedSettingsSigninCoordinator : SigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `signinState` provides the original user sign-in state before starting the
// sign-in flow.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               signinState:(IdentitySigninState)signinState
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_COORDINATOR_H_

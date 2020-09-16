// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_PROTECTED_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

// Methods available only for subclasses of SigninCoordinator.
@interface SigninCoordinator (Protected)

// Runs the sign-in completion callback.
// |signinResult| is the state of sign-in at add account flow completion.
// |identity| is the identity of the added account. Can be nil in the case that
// sign-in is interrupted or canceled before the user has selected an identity.
// |showAdvancedSettingsSignin| is YES if the user wants to open the
// advanced settings signin.
- (void)runCompletionCallbackWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                     identity:(ChromeIdentity*)identity
                   showAdvancedSettingsSignin:(BOOL)showAdvancedSettingsSignin
    NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_PROTECTED_H_

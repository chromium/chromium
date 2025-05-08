// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_COORDINATOR_PROTECTED_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_COORDINATOR_PROTECTED_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

// Methods available only for subclasses of SigninCoordinator.
@interface SigninCoordinator (Protected)

// Runs the sign-in completion callback.
// `signinResult` is the state of sign-in at add account flow completion.
// `completionIdentity` is the info about the sign-in completion.
- (void)runCompletionWithSigninResult:(SigninCoordinatorResult)signinResult
                   completionIdentity:(id<SystemIdentity>)completionIdentity
    NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_COORDINATOR_PROTECTED_H_

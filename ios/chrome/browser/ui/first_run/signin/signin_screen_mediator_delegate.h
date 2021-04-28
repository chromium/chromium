// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class SigninScreenMediator;

// Delegate for the Signin mediator.
@protocol SigninScreenMediatorDelegate

// Notifies the delegate that |mediator| has finished sign in with |result|.
- (void)signinScreenMediator:(SigninScreenMediator*)mediator
    didFinishSigninWithResult:(SigninCoordinatorResult)result;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_DELEGATE_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_enums.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class ChromeIdentity;
@class ChromeIdentityInteractionManager;
class PrefService;

namespace signin {
class IdentityManager;
}

// Delegate that displays screens for the add account flows.
@protocol AddAccountSigninManagerDelegate

// Shows alert modal dialog and interrupts sign-in operation.
// |error| is the error to be displayed.
- (void)addAccountSigninManagerFailedWithError:(NSError*)error;

// Completes the sign-in operation.
// |signinResult| is the state of sign-in at add account flow completion.
// |identity| is the identity of the added account.
- (void)addAccountSigninManagerFinishedWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                               identity:
                                                   (ChromeIdentity*)identity;

@end

// Manager that handles add account and reauthentication UI.
@interface AddAccountSigninManager : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)
    initWithPresentingViewController:(UIViewController*)viewController
          identityInteractionManager:
              (ChromeIdentityInteractionManager*)identityInteractionManager
                         prefService:(PrefService*)prefService
                     identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

// The delegate.
@property(nonatomic, weak) id<AddAccountSigninManagerDelegate> delegate;

// Indicates that the add account sign-in flow was interrupted.
@property(nonatomic, readwrite) BOOL signinInterrupted;

// Displays the add account sign-in flow.
// |signinIntent| is the add account intent.
- (void)showSigninWithIntent:(AddAccountSigninIntent)addAccountSigninIntent;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_

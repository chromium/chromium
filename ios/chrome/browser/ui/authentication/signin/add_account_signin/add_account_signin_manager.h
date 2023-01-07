// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_enums.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class ChromeIdentityInteractionManager;
class PrefService;
@protocol SystemIdentity;

namespace signin {
class IdentityManager;
}

// Delegate that displays screens for the add account flows.
@protocol AddAccountSigninManagerDelegate

// Shows alert modal dialog and interrupts sign-in operation.
// `error` is the error to be displayed.
- (void)addAccountSigninManagerFailedWithError:(NSError*)error;

// Completes the sign-in operation.
// `signinResult` is the state of sign-in at add account flow completion.
// `identity` is the identity of the added account.
- (void)addAccountSigninManagerFinishedWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                               identity:
                                                   (id<SystemIdentity>)identity;

@end

// Manager that handles add account and reauthentication UI.
@interface AddAccountSigninManager : NSObject

// The delegate.
@property(nonatomic, weak) id<AddAccountSigninManagerDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                identityInteractionManager:(ChromeIdentityInteractionManager*)
                                               identityInteractionManager
                               prefService:(PrefService*)prefService
                           identityManager:
                               (signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

// Displays the add account sign-in flow.
// `signinIntent` is the add account intent.
- (void)showSigninWithIntent:(AddAccountSigninIntent)addAccountSigninIntent;

// Interrupts the add account view.
- (void)interruptAddAccountAnimated:(BOOL)animated
                         completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_

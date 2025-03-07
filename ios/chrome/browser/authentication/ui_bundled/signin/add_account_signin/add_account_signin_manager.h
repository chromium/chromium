// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_enums.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

namespace signin {
class IdentityManager;
}

class PrefService;
@protocol SystemIdentityInteractionManager;
@protocol SystemIdentity;

// Result of an add account to device operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Logged as entries for `Signin.AddAccountToDevice.Result` histogram and
// also used as a token for `Signin.AddAccountToDevice.{Result}.Duration`
// histograms.
// Note: This enum is public as it is needed for unit testing.
// LINT.IfChange(SigninAddAccountToDeviceResult)
enum class SigninAddAccountToDeviceResult : int {
  kSuccess = 0,
  kError = 1,
  kCancelledByUser = 2,
  kInterrupted = 3,
  kMaxValue = kInterrupted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:SigninAddAccountToDeviceResult)

// Delegate that displays screens for the add account flows.
@protocol AddAccountSigninManagerDelegate

// Completes the sign-in operation.
//   * `result` the result of the add account flow.
//   * `identity` is the identity of the added account (non-null when `result`
//      is `SigninAddAccountToDeviceResult::kSuccess`).
//   * `error` is the error to be displayed (non-null only when `result` is
//     `SigninAddAccountToDeviceResult::kError`).
- (void)addAccountSigninManagerFinishedWithResult:
            (SigninAddAccountToDeviceResult)result
                                         identity:(id<SystemIdentity>)identity
                                            error:(NSError*)error;

@end

// Manager that handles add account and reauthentication UI.
@interface AddAccountSigninManager : NSObject

// The delegate.
@property(nonatomic, weak) id<AddAccountSigninManagerDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;
// Default initialiser.
- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
                   prefService:(PrefService*)prefService
               identityManager:(signin::IdentityManager*)identityManager
    identityInteractionManager:
        (id<SystemIdentityInteractionManager>)identityInteractionManager
    NS_DESIGNATED_INITIALIZER;

// Displays the add account sign-in flow.
// `signinIntent`: intent for the add account sign-in flow.
- (void)showSigninWithIntent:(AddAccountSigninIntent)signinIntent;

// Interrupts the add account view. `animated` controls whether the dismissal is
// animated.
- (void)interruptAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ADD_ACCOUNT_SIGNIN_ADD_ACCOUNT_SIGNIN_MANAGER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_REAUTH_SIGNIN_REAUTH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_REAUTH_SIGNIN_REAUTH_COORDINATOR_H_

#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/buggy_authentication_view_owner.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace signin_metrics {
enum class AccessPoint;
}

// The result of the reauth flow.
enum class ReauthResult : int {
  // The reauth flow has completed successfully.
  kSuccess = 0,
  // There was an error during the reauth flow.
  kError = 1,
  // The reauth flow was cancelled by the user.
  kCancelledByUser = 2,
  // The reauth flow was cancelled because the coordinator was stopped.
  kInterrupted = 3,
};

// The delegate for the reauth flow.
@protocol SigninReauthCoordinatorDelegate

// The reauth flow has completed with `result`.
- (void)reauthFinishedWithResult:(ReauthResult)result
                          gaiaID:(const GaiaId*)gaiaID;

@end

// Implements a reauthentication flow that asks the user to resolve a persistent
// auth error by entering their credentials again. This should not be confused
// with LocalReauthenticationCoordinator, which checks whether the device
// confirms that the person holding the device is a user having legitimate
// access to sensitive content.
//
// Once started and up to iOS 18, the view may be removed by UIKit without the
// signoutCompletion being called. Use `viewWillPersist` to
// check whether it currently is possible. See crbug.com/395959814.
@interface SigninReauthCoordinator
    : ChromeCoordinator <BuggyAuthenticationViewOwner>

// The delegate to get notified after the flow has completed.
@property(nonatomic, weak) id<SigninReauthCoordinatorDelegate> delegate;

// Designated initializer for SigninReauthCoordinator started from an explicit
// reauthentication UI.
// `identity` - the identity for which the reauthentication flow should be
//            shown.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   account:(const CoreAccountInfo&)account
                         reauthAccessPoint:
                             (signin_metrics::ReauthAccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

// Designated initializer for SigninReauthCoordinator started from a sign-in
// flow. `identity` - the identity for which the reauthentication flow should be
//            shown.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   account:(const CoreAccountInfo&)account
                         signinAccessPoint:
                             (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_REAUTH_SIGNIN_REAUTH_COORDINATOR_H_

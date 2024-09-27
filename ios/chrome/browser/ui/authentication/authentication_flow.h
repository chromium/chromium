// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_H_

#import <Foundation/Foundation.h>

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class AuthenticationFlowPerformer;
class Browser;
@class UIViewController;
@protocol SystemIdentity;

// Handles completion of AuthenticationFlow operations.
@protocol AuthenticationFlowDelegate <NSObject>

// Indicates that a user dialog is presented from the authentication flow.
- (void)didPresentDialog;

// Indicates that a user dialog is dismissed from the authentication flow.
- (void)didDismissDialog;

@end

// `AuthenticationFlow` manages the authentication flow for a given identity.
//
// A new instance of `AuthenticationFlow` should be used each time an identity
// needs to be signed in.
@interface AuthenticationFlow : NSObject<AuthenticationFlowPerformerDelegate>

// Callback to execute when there we know the user won’t have any more
// opportunity to cancel.
@property(nonatomic, strong) void (^userDecisionCompletion)();

// Designated initializer.
// * `browser` is the current browser where the authentication flow is being
//   presented.
// * `accessPoint` is the sign-in access point
// * `postSignInActions` represents the actions to be taken once `identity` is
//   signed in.
// * `presentingViewController` is the top presented view controller.
- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
              postSignInActions:(PostSignInActionSet)postSignInActions
       presentingViewController:(UIViewController*)presentingViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the sign in flow for the identity given in the constructor. Displays
// the signed inconfirmation dialog allowing the user to sign out or configure
// sync.
// It is safe to destroy this authentication flow when `completion` is called.
// `completion` must not be nil.
- (void)startSignInWithCompletion:
    (signin_ui::SigninCompletionCallback)completion;

// * Interrupts the current sign-in operation (if any).
// * Dismiss any UI presented accordingly to `action`.
// * Calls synchronously the completion callback from
// `startSignInWithCompletion` with the sign-in flag set to no.
//
// Does noting if the sign-in flow is already done
- (void)interruptWithAction:(SigninCoordinatorInterrupt)action;

// The delegate.
@property(nonatomic, weak) id<AuthenticationFlowDelegate> delegate;

// Identity to sign-in.
@property(nonatomic, strong, readonly) id<SystemIdentity> identity;

// Sign-in access point
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

// Whether the History Sync Opt-In screen follows after authentication flow
// completes with success.
@property(nonatomic, assign) BOOL precedingHistorySync;

@end

// Private methods in AuthenticationFlow to test.
@interface AuthenticationFlow (TestingAdditions)
- (void)setPerformerForTesting:(AuthenticationFlowPerformer*)performer;
@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_H_

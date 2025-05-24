// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_H_

#import <UIKit/UIKit.h>

#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/signin/model/constants.h"

@protocol AuthenticationFlowDelegate;
class Browser;
@class UIViewController;
@protocol SystemIdentity;

// `AuthenticationFlow` manages the authentication flow for a given identity.
//
// A new instance of `AuthenticationFlow` should be used each time an identity
// needs to be signed in.
@interface AuthenticationFlow : NSObject <AuthenticationFlowPerformerDelegate>

// The object providing the code to execute after the sign-in.
// It is unset after being used once.
@property(nonatomic, weak) id<AuthenticationFlowDelegate> delegate;

// Designated initializer.
// * `browser` is the current browser where the authentication flow is being
//   presented.
// * `accessPoint` is the sign-in access point.
// * `precedingHistorySync` specifies whether the History Sync Opt-In screen
//   follows after the flow completes with success.
// * `postSignInActions` represents the actions to be taken once `identity` is
//   signed in.
// * `presentingViewController` is the top presented view controller.
// * `anchorView` and `anchorRect` is the position that triggered sign-in.
- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
           precedingHistorySync:(BOOL)precedingHistorySync
              postSignInActions:(PostSignInActionSet)postSignInActions
       presentingViewController:(UIViewController*)presentingViewController
                     anchorView:(UIView*)anchorView
                     anchorRect:(CGRect)anchorRect NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the sign in flow for the identity given in the constructor. Displays
// the signed in confirmation dialog allowing the user to sign out or configure
// sync.
// It is safe to destroy this authentication flow when `completion` is called.
// `completion` must not be nil.
- (void)startSignIn;

// * Interrupts the current sign-in operation (if any).
// * Dismiss any UI presented accordingly to `action`.
// * Calls synchronously the completion callback from
// `startSignInWithCompletion` with the sign-in flag set to no.
//
// Does noting if the sign-in flow is already done
- (void)interrupt;

// Identity to sign-in.
@property(nonatomic, strong, readonly) id<SystemIdentity> identity;

// Sign-in access point
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_FLOW_AUTHENTICATION_FLOW_H_

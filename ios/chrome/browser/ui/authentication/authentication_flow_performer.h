// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

class Browser;
@protocol SystemIdentity;

// Performs the sign-in steps and user interactions as part of the sign-in flow.
@interface AuthenticationFlowPerformer : NSObject

// Initializes a new AuthenticationFlowPerformer. `delegate` will be notified
// when each step completes.
- (instancetype)initWithDelegate:
    (id<AuthenticationFlowPerformerDelegate>)delegate NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Cancels any outstanding work and dismisses an alert view (if shown) using
// animation if `animated` is true.
- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion;

// Fetches the managed status for `identity`.
- (void)fetchManagedStatus:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity;

// Signs `identity` with `hostedDomain` into `profile`.
- (void)signInIdentity:(id<SystemIdentity>)identity
         atAccessPoint:(signin_metrics::AccessPoint)accessPoint
      withHostedDomain:(NSString*)hostedDomain
             toProfile:(ProfileIOS*)profile;

// Signs out of `profile` and sends `didSignOut` to the delegate when
// complete.
- (void)signOutProfile:(ProfileIOS*)profile;

// Immediately signs out `profile` without waiting for dependent services.
- (void)signOutImmediatelyFromProfile:(ProfileIOS*)profile;

// Shows a confirmation dialog for signing in to an account managed by
// `hostedDomain`. The confirmation dialog's content will be different depending
// on the status of User Policy.
- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser;

// Completes the post-signin actions. In most cases the action is showing a
// snackbar confirming sign-in with `identity` and an undo button to sign out
// the user.
- (void)completePostSignInActions:(PostSignInActionSet)postSignInActions
                     withIdentity:(id<SystemIdentity>)identity
                          browser:(Browser*)browser;

// Shows `error` to the user and calls `callback` on dismiss.
- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser;

- (void)registerUserPolicy:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity;

- (void)fetchUserPolicy:(ProfileIOS*)profile
            withDmToken:(NSString*)dmToken
               clientID:(NSString*)clientID
     userAffiliationIDs:(NSArray<NSString*>*)userAffiliationIDs
               identity:(id<SystemIdentity>)identity;

@property(nonatomic, weak, readonly) id<AuthenticationFlowPerformerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

@class AuthenticationFlow;
@class InstantSigninMediator;

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

@protocol InstantSigninMediatorDelegate <NSObject>

// Called when the sign-in is over.
- (void)instantSigninMediator:(InstantSigninMediator*)mediator
          didSigninWithResult:(SigninCoordinatorResult)result;

// Called when the sign-in will be done in another profile.
- (void)instantSigninMediatorWillSwitchProfile:(InstantSigninMediator*)mediator;

@end

@interface InstantSigninMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
               continuationProvider:(const ChangeProfileContinuationProvider&)
                                        continuationProvider
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, weak) id<InstantSigninMediatorDelegate> delegate;

// Starts the sign-in flow.
- (void)startSignInOnlyFlowWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow;

// Stops the sign-in flow. Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_MEDIATOR_H_

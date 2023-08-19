// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class AuthenticationFlow;
@class InstantSigninMediator;
enum class SigninCoordinatorInterrupt;

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace syncer {
class SyncService;
}  // namespace syncer

@protocol InstantSigninMediatorDelegate <NSObject>

// Called when the sign-in is over.
- (void)instantSigninMediator:(InstantSigninMediator*)mediator
          didSigninWithResult:(SigninCoordinatorResult)result;

@end

@interface InstantSigninMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                        accessPoint:(signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, weak) id<InstantSigninMediatorDelegate> delegate;

// Starts the sign-in flow.
- (void)startSignInOnlyFlowWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow;

// Disconnect the mediator.
- (void)disconnect;

// Stops the sign-in flow. This method can only be called once, and only after
// `startSignInOnlyFlowWithAuthenticationFlow:` has ben called.
- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_INSTANT_SIGNIN_INSTANT_SIGNIN_MEDIATOR_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
@class ConsistencyPromoSigninMediator;
class PrefService;
@class SigninCompletionInfo;
@protocol SystemIdentity;

namespace signin {
class IdentityManager;
}  // signin

namespace signin_metrics {
enum class AccessPoint : int;
}

// Sign-in error.
typedef NS_ENUM(NSInteger, ConsistencyPromoSigninMediatorError) {
  // Time out error.
  ConsistencyPromoSigninMediatorErrorTimeout,
  // Generic error.
  ConsistencyPromoSigninMediatorErrorGeneric,
};

// Delegate for ConsistencyPromoSigninMediator.
@protocol ConsistencyPromoSigninMediatorDelegate <NSObject>

// Called when the sign-in flow is started. One of the following will be called
// to finish the sign-in flow (once the cookies are available or not, or when
// the flow is cancelled):
// -[id<ConsistencyPromoSigninMediatorDelegate>
// consistencyPromoSigninMediatorSignInDone:withIdentity:]
// -[id<ConsistencyPromoSigninMediatorDelegate>
// consistencyPromoSigninMediatorGenericErrorDidHappen:]
// -[id<ConsistencyPromoSigninMediatorDelegate>
// consistencyPromoSigninMediatorSignInCancelled:]
- (void)consistencyPromoSigninMediatorSigninStarted:
    (ConsistencyPromoSigninMediator*)mediator;

// Called if the sign-in is successful.
- (void)consistencyPromoSigninMediatorSignInDone:
            (ConsistencyPromoSigninMediator*)mediator
                                    withIdentity:(id<SystemIdentity>)identity;

// Called if the sign-in is cancelled.
- (void)consistencyPromoSigninMediatorSignInCancelled:
    (ConsistencyPromoSigninMediator*)mediator;

// Called if there is sign-in error.
- (void)consistencyPromoSigninMediator:(ConsistencyPromoSigninMediator*)mediator
                        errorDidHappen:
                            (ConsistencyPromoSigninMediatorError)error;

@end

// Mediator for ConsistencyPromoSigninCoordinator.
@interface ConsistencyPromoSigninMediator : NSObject

@property(nonatomic, weak) id<ConsistencyPromoSigninMediatorDelegate> delegate;

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                  userPrefService:(PrefService*)userPrefService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint;

// Disconnects the mediator.
- (void)disconnectWithResult:(SigninCoordinatorResult)signinResult;

// Records when an identity is added by a subscreen of the web sign-in dialogs.
- (void)systemIdentityAdded:(id<SystemIdentity>)identity;

// Starts the sign-in flow.
- (void)signinWithAuthenticationFlow:(AuthenticationFlow*)authenticationFlow;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

class AuthenticationService;
class ChromeAccountManagerService;
@class ChromeIdentity;
@class ConsistencyPromoSigninMediator;
class PrefService;
@class SigninCompletionInfo;

namespace signin {
class IdentityManager;
}  // signin

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
// to finish the sign-in flow (once the cookies are available or not):
// -[id<ConsistencyPromoSigninMediatorDelegate>
// consistencyPromoSigninMediatorSignInDone:withIdentity:]
// -[id<ConsistencyPromoSigninMediatorDelegate>
// consistencyPromoSigninMediatorGenericErrorDidHappen:]
- (void)consistencyPromoSigninMediatorSigninStarted:
    (ConsistencyPromoSigninMediator*)mediator;

// Called if the sign-in is successful.
- (void)consistencyPromoSigninMediatorSignInDone:
            (ConsistencyPromoSigninMediator*)mediator
                                    withIdentity:(ChromeIdentity*)identity;

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
                  userPrefService:(PrefService*)userPrefService;

// Disconnects the mediator.
- (void)disconnectWithResult:(SigninCoordinatorResult)signinResult;

// Records when an identity is added by a subscreen of the web sign-in dialogs.
- (void)chromeIdentityAdded:(ChromeIdentity*)identity;

// Starts the sign-in flow.
- (void)signinWithIdentity:(ChromeIdentity*)identity;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_

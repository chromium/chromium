// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>

#import "base/functional/callback_forward.h"
#import "base/ios/block_types.h"
#import "components/signin/public/browser/web_signin_tracker.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

class AccountReconcilor;
@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
@class ConsistencyPromoSigninMediator;
class PrefService;
@protocol SystemIdentity;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace signin_metrics {
enum class AccessPoint : int;
}

// Sign-in error.
typedef NS_ENUM(NSInteger, ConsistencyPromoSigninMediatorError) {
  // Time out error.
  ConsistencyPromoSigninMediatorErrorTimeout,
  // Generic error.
  ConsistencyPromoSigninMediatorErrorGeneric,
  // Auth error.
  ConsistencyPromoSigninMediatorErrorAuth,
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
                            (ConsistencyPromoSigninMediatorError)error
                          withIdentity:(id<SystemIdentity>)identity;

// Called to create a WebSigninTracker object during the web sign-in flow.
- (std::unique_ptr<signin::WebSigninTracker>)
    trackWebSigninWithIdentityManager:(signin::IdentityManager*)identityManager
                    accountReconcilor:(AccountReconcilor*)accountReconcilor
                        signinAccount:(const CoreAccountId&)signin_account
                         withCallback:
                             (const base::RepeatingCallback<void(
                                  signin::WebSigninTracker::Result)>*)callback
                          withTimeout:
                              (const std::optional<base::TimeDelta>&)timeout;

// Returns a ChangeProfileContinuation.
- (ChangeProfileContinuation)changeProfileContinuation;

@end

// Mediator for ConsistencyPromoSigninCoordinator.
@interface ConsistencyPromoSigninMediator : NSObject

@property(nonatomic, weak) id<ConsistencyPromoSigninMediatorDelegate> delegate;

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                accountReconcilor:(AccountReconcilor*)accountReconcilor
                  userPrefService:(PrefService*)userPrefService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint;

// Disconnects the mediator.
- (void)disconnectWithResult:(SigninCoordinatorResult)signinResult;

// Records when an identity is added by a subscreen of the web sign-in dialogs.
- (void)systemIdentityAdded:(id<SystemIdentity>)identity;

// Starts the sign-in flow.
- (void)signinWithAuthenticationFlow:(AuthenticationFlow*)authenticationFlow;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_PROMO_SIGNIN_MEDIATOR_H_

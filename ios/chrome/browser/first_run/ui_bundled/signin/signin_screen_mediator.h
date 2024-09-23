// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
class PrefService;
@protocol SigninScreenConsumer;
namespace signin {
class IdentityManager;
}  // namespace
namespace signin_metrics {
enum class AccessPoint : int;
enum class PromoAction : int;
}  // namespace signin_metrics
namespace syncer {
class SyncService;
}  // syncer
@protocol SystemIdentity;

// Mediator that handles the sign-in operation.
@interface SigninScreenMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<SigninScreenConsumer> consumer;
// The identity currently selected.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;
// Contains the user choice for UMA reporting. This value is set to the default
// value when the coordinator is initialized.
@property(nonatomic, assign) BOOL UMAReportingUserChoice;
// Whether the user tapped on the TOS link.
@property(nonatomic, assign) BOOL TOSLinkWasTapped;
// Whether the user tapped on the UMA link.
@property(nonatomic, assign) BOOL UMALinkWasTapped;
// Whether an account has been added. Must be set externally.
@property(nonatomic, assign) BOOL addedAccount;
// Whether dismiss gestures should be ignored (e.g. swipe away).
@property(nonatomic, assign, readonly) BOOL ignoreDismissGesture;

// The designated initializer.
// `accountManagerService` account manager service.
// `authenticationService` authentication service.
// `localPrefService` application local pref.
// `prefService` user pref.
// `syncService` sync service.
- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                 localPrefService:(PrefService*)localPrefService
                      prefService:(PrefService*)prefService
                      syncService:(syncer::SyncService*)syncService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

// Sign in the selected account.
- (void)startSignInWithAuthenticationFlow:
            (AuthenticationFlow*)authenticationFlow
                               completion:(ProceduralBlock)completion;

// Signs out the user if needed.
- (void)cancelSignInScreenWithCompletion:(ProceduralBlock)completion;

// User attempted to sign-in by either add an account or by tapping on sing-in
// button.
- (void)userAttemptedToSignin;

// Called when the coordinator is finished.
- (void)finishPresentingWithSignIn:(BOOL)signIn;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_

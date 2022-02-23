// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"

#import "base/cancelable_callback.h"
#import "base/threading/thread_task_runner_handle.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sign-in time out duration.
constexpr NSInteger kSigninTimeoutDurationSeconds = 10;

}

@interface ConsistencyPromoSigninMediator () <
    IdentityManagerObserverBridgeDelegate> {
  // Observer for changes to the user's Google identities.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // Closure to trigger the sign-in time out error. This closure has to be
  // canceled if the sign-in is done in time (or fails).
  base::CancelableOnceClosure _signinTimeoutClosure;
}

// List of gaia IDs added by the user with the consistency view.
// This set is used for metrics reasons.
@property(nonatomic, strong) NSMutableSet* addedGaiaIDs;
// Manager for user's Google identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
@property(nonatomic, assign) AuthenticationService* authenticationService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;
@property(nonatomic, assign) PrefService* userPrefService;
// Identity for the sign-in in progress.
@property(nonatomic, assign) ChromeIdentity* signingIdentity;
// Duration before sign-in timeout. The property is overwritten in unittests.
@property(nonatomic, assign, readonly) NSInteger signinTimeoutDurationSeconds;

@end

@implementation ConsistencyPromoSigninMediator

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                  userPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _userPrefService = userPrefService;
    _addedGaiaIDs = [[NSMutableSet alloc] init];
    _identityManagerObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(self.identityManager, self));
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::SHOWN);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
  DCHECK(!self.authenticationService);
  DCHECK(!self.identityManager);
  DCHECK(!self.userPrefService);
  DCHECK(!_identityManagerObserverBridge.get());
}

- (void)disconnectWithResult:(SigninCoordinatorResult)signinResult {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      DCHECK(self.signingIdentity);
      ChromeIdentity* defaultIdentity =
          self.accountManagerService->GetDefaultIdentity();
      DCHECK(defaultIdentity);
      if ([self.addedGaiaIDs containsObject:self.signingIdentity.gaiaID]) {
        // Added identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_ADDED_ACCOUNT);
      } else if ([defaultIdentity isEqual:self.signingIdentity]) {
        // Default identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_DEFAULT_ACCOUNT);
      } else {
        // Other identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT);
      }
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::DISMISSED_BUTTON);
      break;
    }
    case SigninCoordinatorResultInterrupted: {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::DISMISSED_OTHER);
      break;
    }
  }
  _signinTimeoutClosure.Cancel();
  self.accountManagerService = nullptr;
  self.authenticationService = nullptr;
  self.identityManager = nullptr;
  self.userPrefService = nullptr;
  _identityManagerObserverBridge.reset();
}

- (void)chromeIdentityAdded:(ChromeIdentity*)identity {
  [self.addedGaiaIDs addObject:identity.gaiaID];
}

- (void)signinWithIdentity:(ChromeIdentity*)identity {
  self.signingIdentity = identity;
  // Reset dismissal count if the user wants to sign-in.
  self.userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount, 0);
  self.authenticationService->SignIn(identity, nil);
  [self.delegate consistencyPromoSigninMediatorSigninStarted:self];
  DCHECK(self.authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  __weak __typeof(self) weakSelf = self;
  _signinTimeoutClosure.Reset(base::BindOnce(^{
    [weakSelf cancelSigninWithError:ConsistencyPromoSigninMediatorErrorTimeout];
  }));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, _signinTimeoutClosure.callback(),
      base::Seconds(self.signinTimeoutDurationSeconds));
}

#pragma mark - Properties

- (NSInteger)signinTimeoutDurationSeconds {
  return kSigninTimeoutDurationSeconds;
}

#pragma mark - Private

// Cancels sign-in and calls the delegate to display the error.
- (void)cancelSigninWithError:(ConsistencyPromoSigninMediatorError)error {
  if (!self.authenticationService)
    return;
  self.signingIdentity = nil;
  switch (error) {
    case ConsistencyPromoSigninMediatorErrorTimeout:
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::TIMEOUT_ERROR_SHOWN);
      break;
    case ConsistencyPromoSigninMediatorErrorGeneric:
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::GENERIC_ERROR_SHOWN);
      break;
  }
  __weak __typeof(self) weakSelf = self;
  self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN, false, ^() {
    [weakSelf.delegate consistencyPromoSigninMediator:weakSelf
                                       errorDidHappen:error];
  });
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // Since sign-in UI blocks all other Chrome screens until it is dismissed
      // an account change event must come from the consistency sheet.
      // TODO(crbug.com/1081764): Update if sign-in UI becomes non-blocking.
      DCHECK(self.signingIdentity);
      ChromeIdentity* signedInIdentity =
          self.authenticationService->GetPrimaryIdentity(
              signin::ConsentLevel::kSignin);
      DCHECK([signedInIdentity isEqual:self.signingIdentity]);
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Sign out can be triggered from |onAccountsInCookieUpdated:error:|,
      // if there is cookie fetch error.
      return;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
  }
}

- (void)onAccountsInCookieUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                            error:(const GoogleServiceAuthError&)error {
  if (!self.signingIdentity) {
    // TODO(crbug.com/1204528): This case should not happen, but
    // |onAccountsInCookieUpdated:error:| can be called twice when there is an
    // error. Once this bug is fixed, this |if| should be replaced with
    // |DCHECK(!self.alertCoordinator)|.
    return;
  }
  _signinTimeoutClosure.Cancel();
  if (error.state() == GoogleServiceAuthError::State::NONE &&
      self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin) &&
      accountsInCookieJarInfo.signed_in_accounts.size() > 0) {
    // Reset dismissal count.
    self.userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount, 0);
    DCHECK(self.signingIdentity);
    [self.delegate
        consistencyPromoSigninMediatorSignInDone:self
                                    withIdentity:self.signingIdentity];
    return;
  }
  [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorGeneric];
}

@end

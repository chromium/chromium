// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/account_cookie_waiter.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"

@interface ConsistencyPromoSigninMediator () {
  // See waiter docs. Only used if the access point is web sign-in.
  std::unique_ptr<AccountCookieWaiter> _accountCookieWaiter;
  AuthenticationFlow* _authenticationFlow;
  // True if the mediator was initialized with no existing account on device.
  // Kept for metrics reasons.
  BOOL _initializedWithDefaultAccount;
}

// List of gaia IDs added by the user with the consistency view.
// This set is used for metrics reasons.
@property(nonatomic, strong) NSMutableSet* addedGaiaIDs;
// Manager for user's Google identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
@property(nonatomic, assign) AuthenticationService* authenticationService;
@property(nonatomic, assign) PrefService* userPrefService;
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;
// Identity for the sign-in in progress.
@property(nonatomic, weak) id<SystemIdentity> signingIdentity;

@end

@implementation ConsistencyPromoSigninMediator

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
              accountCookieWaiter:
                  (std::unique_ptr<AccountCookieWaiter>)accountCookieWaiter
                  userPrefService:(PrefService*)userPrefService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _accountCookieWaiter = std::move(accountCookieWaiter);
    _userPrefService = userPrefService;
    _accessPoint = accessPoint;
    _addedGaiaIDs = [[NSMutableSet alloc] init];

    _initializedWithDefaultAccount =
        self.accountManagerService->HasIdentities();
    if (_initializedWithDefaultAccount) {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::SHOWN, _accessPoint);
    } else {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::
              SHOWN_WITH_NO_DEVICE_ACCOUNT,
          _accessPoint);
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService && !self.authenticationService &&
         !_accountCookieWaiter && !self.userPrefService)
      << "self.accountManagerService: " << self.accountManagerService
      << ", self.authenticationService: " << self.authenticationService
      << ", self.userPrefService: " << self.userPrefService
      << ", _accountCookieWaiter: " << _accountCookieWaiter;
}

- (void)disconnectWithResult:(SigninCoordinatorResult)signinResult {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      DCHECK(self.signingIdentity);
      id<SystemIdentity> signingIdentity = self.signingIdentity;
      id<SystemIdentity> defaultIdentity =
          self.accountManagerService->GetDefaultIdentity();
      DCHECK(defaultIdentity);
      if (!_initializedWithDefaultAccount) {
        // Added identity, from having no existing account.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_NO_DEVICE_ACCOUNT,
            _accessPoint);
      } else if ([self.addedGaiaIDs containsObject:signingIdentity.gaiaID]) {
        // Added identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_ADDED_ACCOUNT,
            _accessPoint);
      } else if ([defaultIdentity isEqual:signingIdentity]) {
        // Default identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_DEFAULT_ACCOUNT,
            _accessPoint);
      } else {
        // Other identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT,
            _accessPoint);
      }
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::DISMISSED_BUTTON,
          _accessPoint);
      break;
    }
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted: {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::DISMISSED_OTHER,
          _accessPoint);
      break;
    }
  }
  // Abort any ongoing wait.
  _accountCookieWaiter.reset();
  self.accountManagerService = nullptr;
  self.authenticationService = nullptr;
  self.userPrefService = nullptr;
}

- (void)systemIdentityAdded:(id<SystemIdentity>)identity {
  [self.addedGaiaIDs addObject:identity.gaiaID];
}

- (void)signinWithAuthenticationFlow:(AuthenticationFlow*)authenticationFlow {
  _authenticationFlow = authenticationFlow;
  self.signingIdentity = authenticationFlow.identity;
  // Reset dismissal count if the user wants to sign-in.
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
    self.userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount, 0);
  }
  __weak __typeof(self) weakSelf = self;
  [_authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf authenticationFlowCompletedWithSuccess:success];
  }];
  [self.delegate consistencyPromoSigninMediatorSigninStarted:self];
}

#pragma mark - Private

- (void)authenticationFlowCompletedWithSuccess:(BOOL)success {
  DCHECK(_authenticationFlow);
  _authenticationFlow = nil;
  if (!success) {
    [self cancelSigninWithError:
              ConsistencyPromoSigninMediatorErrorFailedToSignin];
    return;
  }

  if (_accessPoint != signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
    // Other entry points don't need to wait for the account cookie, done.
    [self.delegate
        consistencyPromoSigninMediatorSignInDone:self
                                    withIdentity:self.signingIdentity];
    return;
  }

  // Wait for the account cookie to arrive, or an error, or timeout.
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(
      [](__typeof(self) strongSelf, AccountCookieWaiter::Result result) {
        [strongSelf onCookieWaitDoneWithResult:result];
      },
      weakSelf);
  _accountCookieWaiter->Wait(CoreAccountId::FromGaiaId(base::SysNSStringToUTF8(
                                 self.signingIdentity.gaiaID)),
                             std::move(callback));
}

// Cancels sign-in and calls the delegate to display the error.
- (void)cancelSigninWithError:(ConsistencyPromoSigninMediatorError)error {
  if (!self.authenticationService)
    return;
  self.signingIdentity = nil;
  _authenticationFlow = nil;
  switch (error) {
    case ConsistencyPromoSigninMediatorErrorTimeout:
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::TIMEOUT_ERROR_SHOWN,
          _accessPoint);
      break;
    case ConsistencyPromoSigninMediatorErrorGeneric:
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::GENERIC_ERROR_SHOWN,
          _accessPoint);
      break;
    case ConsistencyPromoSigninMediatorErrorFailedToSignin:
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::SIGN_IN_FAILED,
          _accessPoint);
      break;
  }
  __weak __typeof(self) weakSelf = self;
  self.authenticationService->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin, false, ^() {
        [weakSelf.delegate consistencyPromoSigninMediator:weakSelf
                                           errorDidHappen:error];
      });
}

- (void)onCookieWaitDoneWithResult:(AccountCookieWaiter::Result)result {
  switch (result) {
    case AccountCookieWaiter::Result::kAuthError:
      [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorGeneric];
      break;
    case AccountCookieWaiter::Result::kTimeout:
      [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorTimeout];
      break;
    case AccountCookieWaiter::Result::kSuccess:
      if (self.accessPoint ==
          signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
        self.userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount,
                                         0);
      }
      [self.delegate
          consistencyPromoSigninMediatorSignInDone:self
                                      withIdentity:self.signingIdentity];
      break;
  }
}

@end

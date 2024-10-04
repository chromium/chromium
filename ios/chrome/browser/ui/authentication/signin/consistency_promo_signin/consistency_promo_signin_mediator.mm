// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"

#import "base/cancelable_callback.h"
#import "base/task/single_thread_task_runner.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"

namespace {

// Sign-in time out duration.
constexpr base::TimeDelta kSigninTimeout = base::Seconds(10);

}  // namespace

@interface ConsistencyPromoSigninMediator () <
    IdentityManagerObserverBridgeDelegate> {
  // Observer for changes to the user's Google identities.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // Closure to trigger the sign-in time out error. This closure exists to make
  // sure the user doesn't wait too long before to get the cookies available
  // on the web. This is used only when `_accessPoint` is equal to
  // `ACCESS_POINT_WEB_SIGNIN`.
  base::CancelableOnceClosure _cookieTimeoutClosure;
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
@property(nonatomic, assign) signin::IdentityManager* identityManager;
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
                  identityManager:(signin::IdentityManager*)identityManager
                  userPrefService:(PrefService*)userPrefService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _userPrefService = userPrefService;
    _accessPoint = accessPoint;
    _addedGaiaIDs = [[NSMutableSet alloc] init];
    _identityManagerObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(self.identityManager, self));

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
         !self.identityManager && !self.userPrefService &&
         !_identityManagerObserverBridge.get())
      << "self.accountManagerService: " << self.accountManagerService
      << ", self.authenticationService: " << self.authenticationService
      << ", self.identityManager: " << self.identityManager
      << ", self.userPrefService: " << self.userPrefService
      << ", _identityManagerObserverBridge: "
      << _identityManagerObserverBridge.get();
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
  _cookieTimeoutClosure.Cancel();
  self.accountManagerService = nullptr;
  self.authenticationService = nullptr;
  self.identityManager = nullptr;
  self.userPrefService = nullptr;
  _identityManagerObserverBridge.reset();
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
  [_authenticationFlow
      startSignInWithCompletion:^(SigninCoordinatorResult result) {
        [weakSelf authenticationFlowCompletedWithResult:result];
      }];
  [self.delegate consistencyPromoSigninMediatorSigninStarted:self];
}

#pragma mark - Private

- (void)authenticationFlowCompletedWithResult:(SigninCoordinatorResult)result {
  DCHECK(_authenticationFlow);
  _authenticationFlow = nil;
  if (result != SigninCoordinatorResultSuccess) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            IOS_AUTH_FLOW_CANCELLED_OR_FAILED,
        _accessPoint);
    // The error handling and sign-out should be already done in the
    // authentication flow.
    [self.delegate consistencyPromoSigninMediatorSignInCancelled:self];
    return;
  }
  if (_accessPoint == signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
    // `-[ConsistencyPromoSigninMediator onAccountsInCookieUpdated:error:]` will
    // be called when the cookies will be ready, and then the sign-in can be
    // finished. Or `_cookieTimeoutClosure` will be called if it takes too long.
    __weak __typeof(self) weakSelf = self;
    _cookieTimeoutClosure.Reset(base::BindOnce(^{
      [weakSelf
          cancelSigninWithError:ConsistencyPromoSigninMediatorErrorTimeout];
    }));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, _cookieTimeoutClosure.callback(), kSigninTimeout);
    return;
  }
  [self.delegate consistencyPromoSigninMediatorSignInDone:self
                                             withIdentity:self.signingIdentity];
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
  }
  __weak __typeof(self) weakSelf = self;
  self.authenticationService->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin, false, ^() {
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
      // TODO(crbug.com/40691525): Update if sign-in UI becomes non-blocking.
      DCHECK(self.signingIdentity);
      id<SystemIdentity> signedInIdentity =
          self.authenticationService->GetPrimaryIdentity(
              signin::ConsentLevel::kSignin);
      DCHECK([signedInIdentity isEqual:self.signingIdentity]);
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Sign out can be triggered from `onAccountsInCookieUpdated:error:`,
      // if there is cookie fetch error.
      return;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
  }
}

- (void)onAccountsInCookieUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                            error:(const GoogleServiceAuthError&)error {
  if (_authenticationFlow ||
      _accessPoint != signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
    // Ignore if `_authenticationFlow` is in progress since
    // `onAccountsInCookieUpdated` may be called when data is cleared on
    // sign-in.
    // Ignore if the access point is different than WebSignin. Only the web
    // sign-in needs to wait for the cookies.
    return;
  }
  id<SystemIdentity> signingIdentity = self.signingIdentity;
  if (!signingIdentity) {
    // TODO(crbug.com/40764093): This case should not happen, but
    // `onAccountsInCookieUpdated:error:` can be called twice when there is an
    // error. Once this bug is fixed, this `if` should be replaced with
    // `DCHECK(!self.alertCoordinator)`.
    return;
  }
  DCHECK(!_authenticationFlow);
  _cookieTimeoutClosure.Cancel();
  if (error.state() == GoogleServiceAuthError::State::NONE &&
      self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin) &&
      accountsInCookieJarInfo.GetPotentiallyInvalidSignedInAccounts().size() >
          0) {
    // Reset dismissal count.
    if (self.accessPoint ==
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
      self.userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount, 0);
    }
    [self.delegate consistencyPromoSigninMediatorSignInDone:self
                                               withIdentity:signingIdentity];
    return;
  }
  [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorGeneric];
}

@end

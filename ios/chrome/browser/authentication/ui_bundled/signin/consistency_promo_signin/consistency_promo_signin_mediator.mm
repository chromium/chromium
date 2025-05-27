// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"

#import "base/cancelable_callback.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#import "components/prefs/pref_service.h"
#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/browser/web_signin_tracker.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

namespace {

// Sign-in time out duration.
constexpr base::TimeDelta kSigninTimeout = base::Seconds(10);

}  // namespace

@interface ConsistencyPromoSigninMediator () <
    AuthenticationFlowDelegate,
    IdentityManagerObserverBridgeDelegate> {
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<AccountReconcilor> _accountReconcilor;
  raw_ptr<PrefService> _prefService;

  signin_metrics::AccessPoint _accessPoint;

  // List of gaia IDs added by the user with the consistency view. Used for
  // metrics.
  NSMutableSet* _addedGaiaIDs;

  // Identity for the sign-in in progress.
  __weak id<SystemIdentity> _signingIdentity;

  // Observer for changes to the user's Google identities.
  // TODO(crbug.com/395789708): Remove after launching
  //     kEnableIdentityInAuthError.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;

  // Observes AccountReconcilor and account cookies and determines whether the
  // web sign-in flow finished successfully or with an error.
  std::unique_ptr<signin::WebSigninTracker> _webSigninTracker;
  // Closure to trigger the sign-in time out error. This closure exists to make
  // sure the user doesn't wait too long before to get the cookies available
  // on the web. This is used only when `_accessPoint` is equal to
  // `kWebSignin`.
  base::CancelableOnceClosure _cookieTimeoutClosure;
  AuthenticationFlow* _authenticationFlow;
  // True if the mediator was initialized with no existing account on device.
  // Kept for metrics reasons.
  BOOL _initializedWithDefaultAccount;
}

@end

@implementation ConsistencyPromoSigninMediator

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                accountReconcilor:(AccountReconcilor*)accountReconcilor
                  userPrefService:(PrefService*)userPrefService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    CHECK(identityManager);
    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _accountReconcilor = accountReconcilor;
    _prefService = userPrefService;
    _accessPoint = accessPoint;
    _addedGaiaIDs = [[NSMutableSet alloc] init];
    if (!base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError)) {
      _identityManagerObserverBridge =
          std::make_unique<signin::IdentityManagerObserverBridge>(
              _identityManager, self);
    }

    _initializedWithDefaultAccount =
        signin::GetDefaultIdentityOnDevice(_identityManager,
                                           _accountManagerService) != nil;

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
  DCHECK(!_accountManagerService && !_authenticationService &&
         !_identityManager && !_accountReconcilor && !_prefService &&
         !_identityManagerObserverBridge.get())
      << "_accountManagerService: " << _accountManagerService
      << ", _authenticationService: " << _authenticationService
      << ", _identityManager: " << _identityManager
      << ", _accountReconcilor: " << _accountReconcilor
      << ", _prefService: " << _prefService
      << ", _identityManagerObserverBridge: "
      << _identityManagerObserverBridge.get();
}

- (void)disconnectWithResult:(SigninCoordinatorResult)signinResult {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      CHECK(_signingIdentity);
      id<SystemIdentity> signingIdentity = _signingIdentity;
      id<SystemIdentity> defaultIdentity = signin::GetDefaultIdentityOnDevice(
          _identityManager, _accountManagerService);
      DCHECK(defaultIdentity);
      if (!_initializedWithDefaultAccount) {
        // Added identity, from having no existing account.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_NO_DEVICE_ACCOUNT,
            _accessPoint);
      } else if ([_addedGaiaIDs containsObject:signingIdentity.gaiaID]) {
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
    case SigninCoordinatorResultInterrupted:
    case SigninCoordinatorProfileSwitch: {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::DISMISSED_OTHER,
          _accessPoint);
      break;
    }
    case SigninCoordinatorUINotAvailable:
      NOTREACHED();
  }
  _cookieTimeoutClosure.Cancel();
  _accountManagerService = nullptr;
  _authenticationService = nullptr;
  _identityManager = nullptr;
  _accountReconcilor = nullptr;
  _prefService = nullptr;
  _identityManagerObserverBridge.reset();
  _webSigninTracker.reset();
  _authenticationFlow = nil;
}

- (void)systemIdentityAdded:(id<SystemIdentity>)identity {
  [_addedGaiaIDs addObject:identity.gaiaID];
}

- (void)signinWithAuthenticationFlow:(AuthenticationFlow*)authenticationFlow {
  _authenticationFlow = authenticationFlow;
  _signingIdentity = authenticationFlow.identity;
  if (_accessPoint == signin_metrics::AccessPoint::kWebSignin) {
    // Reset dismissal count if the user wants to sign-in.
    _prefService->SetInteger(prefs::kSigninWebSignDismissalCount, 0);
  }
  _authenticationFlow.delegate = self;
  [_authenticationFlow startSignIn];
  [self.delegate consistencyPromoSigninMediatorSigninStarted:self];
}

#pragma mark - AuthenticationFlowDelegate

- (void)authenticationFlowDidSignInInSameProfileWithResult:
    (SigninCoordinatorResult)result {
  if (!_identityManager) {
    // The mediator was already disconnected, nothing to do.
    return;
  }
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
  if (_accessPoint != signin_metrics::AccessPoint::kWebSignin) {
    [self.delegate consistencyPromoSigninMediatorSignInDone:self
                                               withIdentity:_signingIdentity];
    return;
  }
  // For kWebSignin access point, wait for sign-in cookies before reporting
  // success.
  if (base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError)) {
    CoreAccountId accountId = CoreAccountId::FromGaiaId(
        GaiaId(base::SysNSStringToUTF8(_signingIdentity.gaiaID)));
    __weak __typeof(self) weakSelf = self;
    base::RepeatingCallback<void(signin::WebSigninTracker::Result)> callback =
        base::BindRepeating(
            [](__typeof(self) strongSelf,
               signin::WebSigninTracker::Result result) {
              [strongSelf webSigninFinishedWithResult:result];
            },
            weakSelf);
    _webSigninTracker =
        [self.delegate trackWebSigninWithIdentityManager:_identityManager
                                       accountReconcilor:_accountReconcilor
                                           signinAccount:accountId
                                            withCallback:&callback
                                             withTimeout:kSigninTimeout];
    return;
  }
  // `-[ConsistencyPromoSigninMediator onAccountsInCookieUpdated:error:]` will
  // be called when the cookies will be ready, and then the sign-in can be
  // finished. Or `_cookieTimeoutClosure` will be called if it takes too long.
  __weak __typeof(self) weakSelf = self;
  _cookieTimeoutClosure.Reset(base::BindOnce(^{
    [weakSelf cancelSigninWithError:ConsistencyPromoSigninMediatorErrorTimeout];
  }));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, _cookieTimeoutClosure.callback(), kSigninTimeout);
}

- (ChangeProfileContinuation)authenticationFlowWillChangeProfile {
  _authenticationFlow.delegate = nil;
  _authenticationFlow = nil;
  return [self.delegate changeProfileContinuation];
}

#pragma mark - Private

// Called by _webSigninTracker when the result of the web sign-in flow is known.
- (void)webSigninFinishedWithResult:(signin::WebSigninTracker::Result)result {
  _webSigninTracker.reset();
  switch (result) {
    case signin::WebSigninTracker::Result::kSuccess:
      [self.delegate consistencyPromoSigninMediatorSignInDone:self
                                                 withIdentity:_signingIdentity];
      break;
    case signin::WebSigninTracker::Result::kOtherError:
      [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorGeneric];
      break;
    case signin::WebSigninTracker::Result::kAuthError:
      [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorAuth];
      break;
    case signin::WebSigninTracker::Result::kTimeout:
      [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorTimeout];
      break;
  }
}

// Cancels sign-in and calls the delegate to display the error.
- (void)cancelSigninWithError:(ConsistencyPromoSigninMediatorError)error {
  if (!_authenticationService) {
    return;
  }
  id<SystemIdentity> signinIdentity = _signingIdentity;
  _signingIdentity = nil;
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
    case ConsistencyPromoSigninMediatorErrorAuth:
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::AUTH_ERROR_SHOWN,
          _accessPoint);
      break;
  }
  __weak __typeof(self) weakSelf = self;
  _authenticationService->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin, ^() {
        [weakSelf.delegate consistencyPromoSigninMediator:weakSelf
                                           errorDidHappen:error
                                             withIdentity:signinIdentity];
      });
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  CHECK(!base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError));
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // Since sign-in UI blocks all other Chrome screens until it is dismissed
      // an account change event must come from the consistency sheet.
      // TODO(crbug.com/40691525): Update if sign-in UI becomes non-blocking.
      CHECK(_signingIdentity);
      id<SystemIdentity> signedInIdentity =
          _authenticationService->GetPrimaryIdentity(
              signin::ConsentLevel::kSignin);
      DCHECK([signedInIdentity isEqual:_signingIdentity]);
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
  CHECK(!base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError));
  if (_authenticationFlow ||
      _accessPoint != signin_metrics::AccessPoint::kWebSignin) {
    // Ignore if `_authenticationFlow` is in progress since
    // `onAccountsInCookieUpdated` may be called when data is cleared on
    // sign-in.
    // Ignore if the access point is different than WebSignin. Only the web
    // sign-in needs to wait for the cookies.
    return;
  }
  id<SystemIdentity> signingIdentity = _signingIdentity;
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
      _authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin) &&
      accountsInCookieJarInfo.GetPotentiallyInvalidSignedInAccounts().size() >
          0) {
    // Reset dismissal count.
    if (_accessPoint == signin_metrics::AccessPoint::kWebSignin) {
      _prefService->SetInteger(prefs::kSigninWebSignDismissalCount, 0);
    }
    [self.delegate consistencyPromoSigninMediatorSignInDone:self
                                               withIdentity:signingIdentity];
    return;
  }
  [self cancelSigninWithError:ConsistencyPromoSigninMediatorErrorGeneric];
}

@end

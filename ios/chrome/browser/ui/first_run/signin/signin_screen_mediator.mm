// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator.h"

#import "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninScreenMediator () {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

@property(nonatomic, assign) BOOL showFREConsent;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// Authentication service for sign in.
@property(nonatomic, assign) AuthenticationService* authenticationService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// Application local pref.
@property(nonatomic, assign) PrefService* localPrefService;
// User pref.
@property(nonatomic, assign) PrefService* prefService;
// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;
// Logger used to record sign in metrics.
@property(nonatomic, strong) UserSigninLogger* logger;
// Whether the user attempted to sign in (the attempt can be successful, failed
// or canceled).
@property(nonatomic, assign, readwrite)
    first_run::SignInAttemptStatus attemptStatus;
// Whether there was existing accounts when the screen was presented.
@property(nonatomic, assign) BOOL hadIdentitiesAtStartup;

@end

@implementation SigninScreenMediator

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                 localPrefService:(PrefService*)localPrefService
                      prefService:(PrefService*)prefService
                      syncService:(syncer::SyncService*)syncService
                   showFREConsent:(BOOL)showFREConsent {
  self = [super init];
  if (self) {
    DCHECK(accountManagerService);
    DCHECK(authenticationService);
    DCHECK(localPrefService);
    DCHECK(prefService);
    DCHECK(syncService);

    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _identityManager = identityManager;
    _localPrefService = localPrefService;
    _prefService = prefService;
    _syncService = syncService;
    _showFREConsent = showFREConsent;
    _hadIdentitiesAtStartup = self.accountManagerService->HasIdentities();
    if (showFREConsent) {
      _logger = [[FirstRunSigninLogger alloc]
            initWithPromoAction:signin_metrics::PromoAction::
                                    PROMO_ACTION_NO_SIGNIN_PROMO
          accountManagerService:accountManagerService];
    } else {
      // SigninScreenMediator supports only FRE or force sign-in.
      DCHECK_EQ(AuthenticationService::ServiceStatus::SigninForcedByPolicy,
                _authenticationService->GetServiceStatus());
      _logger = [[UserSigninLogger alloc]
            initWithAccessPoint:signin_metrics::AccessPoint::
                                    ACCESS_POINT_FORCED_SIGNIN
                    promoAction:signin_metrics::PromoAction::
                                    PROMO_ACTION_NO_SIGNIN_PROMO
          accountManagerService:accountManagerService];
    }
    [_logger logSigninStarted];
    if (self.showFREConsent) {
      base::UmaHistogramEnumeration("FirstRun.Stage",
                                    first_run::kWelcomeAndSigninScreenStart);
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
  DCHECK(!self.authenticationService);
  DCHECK(!self.localPrefService);
  DCHECK(!self.prefService);
  DCHECK(!self.syncService);
}

- (void)disconnect {
  [self.logger disconnect];
  self.accountManagerService = nullptr;
  self.authenticationService = nullptr;
  self.identityManager = nullptr;
  self.localPrefService = nullptr;
  self.prefService = nullptr;
  self.syncService = nullptr;
  _accountManagerServiceObserver.reset();
}

- (void)startSignInWithAuthenticationFlow:
            (AuthenticationFlow*)authenticationFlow
                               completion:(ProceduralBlock)completion {
  [self userAttemptedToSignin];
  RecordMetricsReportingDefaultState();
  [self.consumer setUIEnabled:NO];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock startSignInCompletion = ^() {
    [authenticationFlow startSignInWithCompletion:^(BOOL success) {
      [weakSelf.consumer setUIEnabled:YES];
      if (!success)
        return;
      [weakSelf.logger
          logSigninCompletedWithResult:SigninCoordinatorResultSuccess
                          addedAccount:weakSelf.addedAccount
                 advancedSettingsShown:NO];
      if (completion)
        completion();
    }];
  };
  ChromeIdentity* primaryIdentity =
      self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
  if (primaryIdentity && ![primaryIdentity isEqual:self.selectedIdentity]) {
    // This case is possible if the user signs in with the FRE, and quits Chrome
    // without completed the FRE. And the user starts Chrome again.
    // See crbug.com/1312449.
    // TODO(crbug.com/1314012): Need test for this case.
    self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN,
                                        /*force_clear_browsing_data=*/true,
                                        startSignInCompletion);
    return;
  }
  startSignInCompletion();
}

- (void)cancelSignInScreenWithCompletion:(ProceduralBlock)completion {
  if (!self.authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    if (completion) {
      completion();
    }
    return;
  }
  [self.consumer setUIEnabled:NO];
  // This case is possible if the user signs in with the FRE, and quits Chrome
  // without completed the FRE. And the user starts Chrome again.
  // See crbug.com/1312449.
  // TODO(crbug.com/1314012): Need test for this case.
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock signOutCompletion = ^() {
    [weakSelf.consumer setUIEnabled:YES];
    if (completion) {
      completion();
    }
  };
  self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN,
                                      /*force_clear_browsing_data=*/true,
                                      signOutCompletion);
}

- (void)userAttemptedToSignin {
  DCHECK_NE(self.attemptStatus,
            first_run::SignInAttemptStatus::SKIPPED_BY_POLICY);
  DCHECK_NE(self.attemptStatus, first_run::SignInAttemptStatus::NOT_SUPPORTED);
  self.attemptStatus = first_run::SignInAttemptStatus::ATTEMPTED;
}

- (void)finishPresentingWithSignIn:(BOOL)signIn {
  if (self.TOSLinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreTOSLinkTapped"));
  }
  if (self.UMALinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreUMALinkTapped"));
  }
  if (self.showFREConsent) {
    first_run::FirstRunStage firstRunStage =
        signIn ? first_run::kWelcomeAndSigninScreenCompletionWithSignIn
               : first_run::kWelcomeAndSigninScreenCompletionWithoutSignIn;
    self.localPrefService->SetBoolean(metrics::prefs::kMetricsReportingEnabled,
                                      self.UMAReportingUserChoice);
    base::UmaHistogramEnumeration("FirstRun.Stage", firstRunStage);
    RecordFirstRunSignInMetrics(self.identityManager, self.attemptStatus,
                                self.hadIdentitiesAtStartup);
  }
}

#pragma mark - Properties

- (void)setConsumer:(id<SigninScreenConsumer>)consumer {
  DCHECK(consumer);
  DCHECK(!_consumer);
  _consumer = consumer;
  BOOL signinForcedOrAvailable = NO;
  switch (self.authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_ATTEMPTED;
      signinForcedOrAvailable = YES;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusForced;
      break;
    case AuthenticationService::ServiceStatus::SigninAllowed:
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_ATTEMPTED;
      signinForcedOrAvailable = YES;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusAvailable;
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
      // This is possible only if FirstRun is triggered by the preferences to
      // test FRE.
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_ATTEMPTED;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusDisabled;
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusDisabled;
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_SUPPORTED;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusDisabled;
      break;
  }
  self.consumer.isManaged = IsApplicationManaged();
  if (!self.showFREConsent) {
    self.consumer.screenIntent = SigninScreenConsumerScreenIntentSigninOnly;
  } else {
    BOOL metricReportingDisabled =
        self.localPrefService->IsManagedPreference(
            metrics::prefs::kMetricsReportingEnabled) &&
        !self.localPrefService->GetBoolean(
            metrics::prefs::kMetricsReportingEnabled);
    self.consumer.screenIntent =
        metricReportingDisabled
            ? SigninScreenConsumerScreenIntentWelcomeWithoutUMAAndSignin
            : SigninScreenConsumerScreenIntentWelcomeAndSignin;
  }
  if (signinForcedOrAvailable) {
    self.selectedIdentity = self.accountManagerService->GetDefaultIdentity();
  }
}

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if ([self.selectedIdentity isEqual:selectedIdentity]) {
    return;
  }
  // nil is allowed only if there is no other identity.
  DCHECK(selectedIdentity || !self.accountManagerService->HasIdentities());
  _selectedIdentity = selectedIdentity;

  [self updateConsumerIdentity];
}

#pragma mark - Private

- (void)updateConsumerIdentity {
  switch (self.authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return;
  }
  if (!self.selectedIdentity) {
    [self.consumer noIdentityAvailable];
  } else {
    UIImage* avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
        self.selectedIdentity, IdentityAvatarSize::DefaultLarge);
    [self.consumer
        setSelectedIdentityUserName:self.selectedIdentity.userFullName
                              email:self.selectedIdentity.userEmail
                          givenName:self.selectedIdentity.userGivenName
                             avatar:avatar];
  }
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (!self.selectedIdentity ||
      !self.accountManagerService->IsValidIdentity(self.selectedIdentity)) {
    self.selectedIdentity = self.accountManagerService->GetDefaultIdentity();
  }
}

- (void)identityChanged:(ChromeIdentity*)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateConsumerIdentity];
  }
}

@end

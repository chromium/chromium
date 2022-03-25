// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator.h"

#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
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
// Application local pref.
@property(nonatomic, assign) PrefService* localPrefService;
// User pref.
@property(nonatomic, assign) PrefService* prefService;
// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;
// The identity currently selected.
@property(nonatomic, strong) ChromeIdentity* selectedIdentity;

@end

@implementation SigninScreenMediator

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                        authenticationService:
                            (AuthenticationService*)authenticationService
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

    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _localPrefService = localPrefService;
    _prefService = prefService;
    _syncService = syncService;
    _showFREConsent = showFREConsent;
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
  self.accountManagerService = nullptr;
  self.authenticationService = nullptr;
  self.localPrefService = nullptr;
  self.prefService = nullptr;
  self.syncService = nullptr;
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<SigninScreenConsumer>)consumer {
  DCHECK(consumer);
  DCHECK(!_consumer);
  _consumer = consumer;
  BOOL signinForcedOrAvailable = NO;
  switch (_authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
      signinForcedOrAvailable = YES;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusForced;
      break;
    case AuthenticationService::ServiceStatus::SigninAllowed:
      signinForcedOrAvailable = YES;
      _consumer.signinStatus = SigninScreenConsumerSigninStatusAvailable;
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      _consumer.signinStatus = SigninScreenConsumerSigninStatusDisabled;
      break;
  }
  self.consumer.managedEnabled =
      GetEnterpriseSignInRestrictions(self.authenticationService,
                                      self.prefService, self.syncService) !=
      kNoEnterpriseRestriction;
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
  switch (_authenticationService->GetServiceStatus()) {
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

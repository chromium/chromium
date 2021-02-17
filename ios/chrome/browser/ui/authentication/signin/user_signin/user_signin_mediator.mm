// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"

#include <memory>

#include "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UserSigninMediator ()

// Manager for the authentication flow.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// Manager for user's Google identities.
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// Auditor for user consent.
@property(nonatomic, assign) consent_auditor::ConsentAuditor* consentAuditor;
// Chrome interface to the iOS shared authentication library.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Manager for user consent.
@property(nonatomic, assign)
    unified_consent::UnifiedConsentService* unifiedConsentService;
// Service that allows for configuring sync.
@property(nonatomic, assign) SyncSetupService* syncSetupService;

@end

@implementation UserSigninMediator

#pragma mark - Public

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                   consentAuditor:
                       (consent_auditor::ConsentAuditor*)consentAuditor
            unifiedConsentService:
                (unified_consent::UnifiedConsentService*)unifiedConsentService
                 syncSetupService:(SyncSetupService*)syncSetupService {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
    _consentAuditor = consentAuditor;
    _authenticationService = authenticationService;
    _unifiedConsentService = unifiedConsentService;
    _syncSetupService = syncSetupService;
  }
  return self;
}

- (void)authenticateWithIdentity:(ChromeIdentity*)identity
              authenticationFlow:(AuthenticationFlow*)authenticationFlow {
  DCHECK(!self.authenticationFlow);

  self.authenticationFlow = authenticationFlow;
  __weak UserSigninMediator* weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf onAccountSigninCompletion:success identity:identity];
  }];
}

- (void)cancelSignin {
  if (self.isAuthenticationInProgress) {
    [self cancelAndDismissAuthenticationFlowAnimated:NO];
  } else {
    [self.delegate userSigninMediatorSigninFinishedWithResult:
                       SigninCoordinatorResultCanceledByUser];
  }
}

- (void)cancelAndDismissAuthenticationFlowAnimated:(BOOL)animated {
  if (!self.isAuthenticationInProgress) {
    return;
  }

  [self.authenticationFlow cancelAndDismissAnimated:animated];
  self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN,
                                      /*force_clear_browsing_data=*/false, nil);
}

#pragma mark - Private

- (BOOL)isAuthenticationInProgress {
  return self.authenticationFlow != nil;
}

- (void)onAccountSigninCompletion:(BOOL)success
                         identity:(ChromeIdentity*)identity {
  self.authenticationFlow = nil;
  if (success) {
    [self signinCompletedWithIdentity:identity];
  } else {
    [self.delegate userSigninMediatorSigninFailed];
  }
}

// Starts the sync engine only if the user tapped on "YES, I'm in", and closes
// the sign-in view.
- (void)signinCompletedWithIdentity:(ChromeIdentity*)identity {
  // The consent has to be given as soon as the user is signed in. Even when
  // they open the settings through the link.
  self.unifiedConsentService->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  sync_pb::UserConsentTypes::SyncConsent syncConsent;
  syncConsent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                             UserConsentTypes_ConsentStatus_GIVEN);

  int consentConfirmationId =
      [self.delegate userSigninMediatorGetConsentConfirmationId];
  syncConsent.set_confirmation_grd_id(consentConfirmationId);

  std::vector<int> consentTextIds =
      [self.delegate userSigninMediatorGetConsentStringIds];
  for (int id : consentTextIds) {
    syncConsent.add_description_grd_ids(id);
  }

  CoreAccountId coreAccountId = self.identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8([identity gaiaID]),
      base::SysNSStringToUTF8([identity userEmail]));
  self.consentAuditor->RecordSyncConsent(coreAccountId, syncConsent);
  self.authenticationService->GrantSyncConsent(identity);

  BOOL settingsLinkWasTapped =
      [self.delegate userSigninMediatorGetSettingsLinkWasTapped];
  if (!settingsLinkWasTapped) {
    // FirstSetupComplete flag should be turned on after the authentication
    // service has granted user consent to start Sync.
    self.syncSetupService->SetFirstSetupComplete(
        syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
    self.syncSetupService->CommitSyncChanges();
  }

  [self.delegate userSigninMediatorSigninFinishedWithResult:
                     SigninCoordinatorResultSuccess];
}

@end

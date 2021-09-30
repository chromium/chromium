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
#include "ios/chrome/browser/procedural_block_types.h"
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
// Service that helps reseting the user state.
@property(nonatomic, assign) syncer::SyncService* syncService;

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
                 syncSetupService:(SyncSetupService*)syncSetupService
                      syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
    _consentAuditor = consentAuditor;
    _authenticationService = authenticationService;
    _unifiedConsentService = unifiedConsentService;
    _syncSetupService = syncSetupService;
    _syncService = syncService;
  }
  return self;
}

- (void)authenticateWithIdentity:(ChromeIdentity*)identity
              authenticationFlow:(AuthenticationFlow*)authenticationFlow {
  DCHECK(!self.authenticationFlow);

  self.authenticationFlow = authenticationFlow;
  __weak UserSigninMediator* weakSelf = self;
  BOOL settingsLinkWasTapped =
      [self.delegate userSigninMediatorGetSettingsLinkWasTapped];
  ProceduralBlockWithBool completion = ^(BOOL success) {
    if (settingsLinkWasTapped) {
      [weakSelf
          onAccountSigninCompletionForAdvancedSettingsWithSuccess:success];
    } else {
      // Otherwise, the user tapped "Yes, I'm in".
      [weakSelf onAccountSigninCompletion:success identity:identity];
    }
  };
  [self.authenticationFlow startSignInWithCompletion:completion];
}

- (void)onAccountSigninCompletionForAdvancedSettingsWithSuccess:(BOOL)success {
  self.authenticationFlow = nil;
  if (success) {
    // This will display the advanced sync settings.
    [self.delegate userSigninMediatorSigninFinishedWithResult:
                       SigninCoordinatorResultSuccess];
    // We need to set Sync requested in order to display the preferences
    // correctly and differentiate the special state where the user is
    // signed in, but the sync feature can't start yet.
    self.syncService->GetUserSettings()->SetSyncRequested(true);
  } else {
    [self.delegate userSigninMediatorSigninFailed];
  }
}

- (void)cancelSignin {
  // Cancelling the authentication flow has the side effect of setting
  // |self.isAuthenticationInProgress| to false.
  // Ensure these conditions are handled separately by using a BOOL which
  // retains the initial authentication state. This way, the mediator does not
  // call sign-in finished if sign-in was in progress.
  BOOL shouldNotCallSigninFinishedOnDelegate = self.isAuthenticationInProgress;
  [self cancelAndDismissAuthenticationFlowAnimated:NO];
  if (!shouldNotCallSigninFinishedOnDelegate) {
    [self.delegate userSigninMediatorSigninFinishedWithResult:
                       SigninCoordinatorResultCanceledByUser];
  }
}

- (void)cancelAndDismissAuthenticationFlowAnimated:(BOOL)animated {
  [self.authenticationFlow cancelAndDismissAnimated:animated];
  [self revertToSigninStateOnStart];
}

#pragma mark - Private

// For users that cancel the sign-in flow, revert to the sign-in
// state prior to starting sign-in coordinators.
- (void)revertToSigninStateOnStart {
  self.syncService->GetUserSettings()->SetSyncRequested(false);
  switch (self.delegate.signinStateOnStart) {
    case IdentitySigninStateSignedOut: {
      self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN,
                                          /*force_clear_browsing_data=*/false,
                                          nil);
      break;
    }
    case IdentitySigninStateSignedInWithSyncDisabled: {
      DCHECK(!self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSync));
      // Call StopAndClear() to clear the encryption passphrase, in case the
      // user entered it before canceling the sync opt-in flow.
      _syncService->StopAndClear();
      break;
    }
    case IdentitySigninStateSignedInWithSyncEnabled: {
      // Switching accounts is not possible without sign-out.
      NOTREACHED();
      break;
    }
  }
}

- (BOOL)isAuthenticationInProgress {
  return self.authenticationFlow != nil;
}

// Called when signin is complete, after tapping "Yes, I'm in".
- (void)onAccountSigninCompletion:(BOOL)success
                         identity:(ChromeIdentity*)identity {
  self.authenticationFlow = nil;
  if (!success) {
    [self.delegate userSigninMediatorSigninFailed];
    return;
  }
  [self signinCompletedWithIdentity:identity];
}

// Grants and records Sync consent, and finishes the Sync setup flow.
- (void)signinCompletedWithIdentity:(ChromeIdentity*)identity {
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

  // FirstSetupComplete flag should be turned on after the authentication
  // service has granted user consent to start Sync when tapping "Yes, I'm in."
  self.syncSetupService->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  self.syncSetupService->CommitSyncChanges();

  [self.delegate userSigninMediatorSigninFinishedWithResult:
                     SigninCoordinatorResultSuccess];
}

@end

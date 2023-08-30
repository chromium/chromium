// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_mediator.h"

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/service/sync_service.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_consumer.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_mediator_delegate.h"

@interface TangibleSyncMediator () <ChromeAccountManagerServiceObserver,
                                    IdentityManagerObserverBridgeDelegate>

@end

@implementation TangibleSyncMediator {
  AuthenticationService* _authenticationService;
  ChromeAccountManagerService* _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Auditor for user consent.
  consent_auditor::ConsentAuditor* _consentAuditor;
  // Manager for user's Google identities.
  signin::IdentityManager* _identityManager;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Manager for the authentication flow.
  AuthenticationFlow* _authenticationFlow;
  // Sync service.
  syncer::SyncService* _syncService;
  // Service that allows for configuring sync.
  SyncSetupService* _syncSetupService;
  // Manager for user consent.
  unified_consent::UnifiedConsentService* _unifiedConsentService;
  // Sync opt-in access point.
  signin_metrics::AccessPoint _accessPoint;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
      chromeAccountManagerService:
          (ChromeAccountManagerService*)chromeAccountManagerService
                   consentAuditor:
                       (consent_auditor::ConsentAuditor*)consentAuditor
                  identityManager:(signin::IdentityManager*)identityManager
                      syncService:(syncer::SyncService*)syncService
                 syncSetupService:(SyncSetupService*)syncSetupService
            unifiedConsentService:
                (unified_consent::UnifiedConsentService*)unifiedConsentService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    CHECK(authenticationService);
    CHECK(chromeAccountManagerService);
    CHECK(consentAuditor);
    CHECK(identityManager);
    CHECK(syncService);
    CHECK(syncSetupService);
    CHECK(unifiedConsentService);
    _authenticationService = authenticationService;
    _accountManagerService = chromeAccountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _consentAuditor = consentAuditor;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _syncService = syncService;
    _syncSetupService = syncSetupService;
    _unifiedConsentService = unifiedConsentService;
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)disconnect {
  _accountManagerServiceObserver.reset();
  _identityManagerObserver.reset();
  self.consumer = nil;
  _authenticationService = nullptr;
  _accountManagerService = nullptr;
  _consentAuditor = nullptr;
  _identityManager = nullptr;
  _syncService = nullptr;
  _syncSetupService = nullptr;
  _unifiedConsentService = nullptr;
}

- (void)startSyncWithConfirmationID:(const int)confirmationID
                           consentIDs:(NSArray<NSNumber*>*)consentIDs
                   authenticationFlow:(AuthenticationFlow*)authenticationFlow
    advancedSyncSettingsLinkWasTapped:(BOOL)advancedSyncSettingsLinkWasTapped {
  DCHECK(!_authenticationFlow);

  [self.delegate tangibleSyncMediator:self UIEnabled:NO];

  // Local copy to be captured.
  NSArray<NSNumber*>* consentIDsCopy = [consentIDs copy];

  _authenticationFlow = authenticationFlow;
  __weak __typeof(self) weakSelf = self;
  [_authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf signinCompletedWithSuccess:success
                          confirmationID:confirmationID
                              consentIDs:consentIDsCopy
               advancedSettingsRequested:advancedSyncSettingsLinkWasTapped];
  }];
}

#pragma mark - Properties

- (void)setConsumer:(id<TangibleSyncConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity) {
    // This can happen if identity is removed from the device when the tangible
    // sync is opened. There is no point to update the UI since the dialog will
    // be automatically closed (see -[TangibleSyncMediator
    // onPrimaryAccountChanged:].
    return;
  }
  [self updateAvatarImageWithIdentity:identity];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  id<SystemIdentity> primaryIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if ([primaryIdentity isEqual:identity]) {
    [self updateAvatarImageWithIdentity:identity];
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    // In rare cases, the auth flow may change the primary account.
    // Ignore any primary account cleared event if a sign-in operation
    // is in progress.
    if (_authenticationFlow) {
      return;
    }
    [self.delegate tangibleSyncMediatorUserRemoved:self];
  }
}

#pragma mark - Private

// Updates the avatar image for the consumer from `identity`.
- (void)updateAvatarImageWithIdentity:(id<SystemIdentity>)identity {
  UIImage* image = _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Large);
  self.consumer.primaryIdentityAvatarImage = image;
  NSString* accessibilityLabel = nil;
  if (identity.userFullName.length == 0) {
    accessibilityLabel = identity.userEmail;
  } else {
    accessibilityLabel = [NSString
        stringWithFormat:@"%@ %@", identity.userFullName, identity.userEmail];
  }
  DCHECK(accessibilityLabel);
  self.consumer.primaryIdentityAvatarAccessibilityLabel = accessibilityLabel;
}

// Callback used when the sign in flow is complete, with `success`.
- (void)signinCompletedWithSuccess:(BOOL)success
                    confirmationID:(const int)confirmationID
                        consentIDs:(NSArray<NSNumber*>*)consentIDs
         advancedSettingsRequested:(BOOL)advancedSettingsRequested {
  _authenticationFlow = nil;
  [self.delegate tangibleSyncMediator:self UIEnabled:YES];

  if (!success) {
    return;
  }

  // The user does not give Sync Consent if the Advanced Settings link is
  // tapped.
  if (advancedSettingsRequested) {
    // Sync has to be set as requested in order to display the preferences
    // correctly and differentiate the special state where the user is signed
    // in, but the sync feature can't start yet.
    _syncService->SetSyncFeatureRequested();
  } else {
    // TODO(crbug.com/1254359): Dedupe duplicated code, here and in
    // user_signin_mediator.
    id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    DCHECK(identity);

    sync_pb::UserConsentTypes::SyncConsent syncConsent;
    syncConsent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                               UserConsentTypes_ConsentStatus_GIVEN);

    DCHECK_NE(confirmationID, 0);
    syncConsent.set_confirmation_grd_id(confirmationID);
    DCHECK_NE(consentIDs.count, 0ul);
    for (NSNumber* consentID in consentIDs) {
      syncConsent.add_description_grd_ids([consentID intValue]);
    }

    CoreAccountId coreAccountId = _identityManager->PickAccountIdForAccount(
        base::SysNSStringToUTF8(identity.gaiaID),
        base::SysNSStringToUTF8(identity.userEmail));
    _consentAuditor->RecordSyncConsent(coreAccountId, syncConsent);
    _authenticationService->GrantSyncConsent(identity, _accessPoint);

    _unifiedConsentService->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

    // Turn on FirstSetupComplete flag after the authentication service has
    // granted user consent to start Sync.
    _syncSetupService->SetInitialSyncFeatureSetupComplete(
        syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

    _syncSetupService->CommitSyncChanges();
  }

  [self.delegate tangibleSyncMediatorDidSuccessfulyFinishSignin:self];
}

@end

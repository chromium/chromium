// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncScreenMediator ()

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

@implementation SyncScreenMediator

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

- (void)startSync {
  sync_pb::UserConsentTypes::SyncConsent syncConsent;
  syncConsent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                             UserConsentTypes_ConsentStatus_GIVEN);

  syncConsent.set_confirmation_grd_id(
      IDS_IOS_FIRST_RUN_SYNC_SCREEN_PRIMARY_ACTION);

  ChromeIdentity* identity =
      self.authenticationService->GetAuthenticatedIdentity();
  DCHECK(identity);

  CoreAccountId coreAccountId = self.identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8([identity gaiaID]),
      base::SysNSStringToUTF8([identity userEmail]));
  self.consentAuditor->RecordSyncConsent(coreAccountId, syncConsent);
  self.authenticationService->GrantSyncConsent(identity);

  // Turn on FirstSetupComplete flag after the authentication service has
  // granted user consent to start Sync.
  self.syncSetupService->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  self.syncSetupService->CommitSyncChanges();
}

@end

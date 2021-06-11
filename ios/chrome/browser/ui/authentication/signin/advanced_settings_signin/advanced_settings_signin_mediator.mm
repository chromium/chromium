// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_mediator.h"

#import "base/metrics/user_metrics.h"
#import "components/sync/driver/sync_service.h"
#import "components/unified_consent/unified_consent_metrics.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using syncer::SyncFirstSetupCompleteSource;
using unified_consent::metrics::RecordSyncSetupDataTypesHistrogam;

@interface AdvancedSettingsSigninMediator ()

@property(nonatomic, assign, readonly) SyncSetupService* syncSetupService;
@property(nonatomic, assign, readonly)
    AuthenticationService* authenticationService;
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;
// Browser state preference service.
@property(nonatomic, assign, readonly) PrefService* prefService;

@end

@implementation AdvancedSettingsSigninMediator

#pragma mark - Public

- (instancetype)initWithSyncSetupService:(SyncSetupService*)syncSetupService
                   authenticationService:
                       (AuthenticationService*)authenticationService
                             syncService:(syncer::SyncService*)syncService
                             prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    DCHECK(syncSetupService);
    DCHECK(authenticationService);
    DCHECK(syncService);
    DCHECK(prefService);
    _syncSetupService = syncSetupService;
    _authenticationService = authenticationService;
    _syncService = syncService;
    _prefService = prefService;
  }
  return self;
}

- (void)saveUserPreferenceForSigninResult:
    (SigninCoordinatorResult)signinResult {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      RecordAction(
          UserMetricsAction("Signin_Signin_ConfirmAdvancedSyncSettings"));
      RecordSyncSetupDataTypesHistrogam(self.syncService->GetUserSettings(),
                                        self.prefService);
      if (self.syncSetupService->CanSyncFeatureStart()) {
        // FirstSetupComplete flag should be only turned on when the user agrees
        // to start Sync.
        self.syncSetupService->PrepareForFirstSyncSetup();
        self.syncSetupService->SetFirstSetupComplete(
            SyncFirstSetupCompleteSource::ADVANCED_FLOW_CONFIRM);
      }
      break;
    }
    case SigninCoordinatorResultCanceledByUser:
      RecordAction(
          UserMetricsAction("Signin_Signin_ConfirmCancelAdvancedSyncSettings"));
      self.syncSetupService->CommitSyncChanges();
      self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN,
                                          /*force_clear_browsing_data=*/false,
                                          nil);
      break;
    case SigninCoordinatorResultInterrupted:
      RecordAction(
          UserMetricsAction("Signin_Signin_AbortAdvancedSyncSettings"));
      break;
  }
}

@end

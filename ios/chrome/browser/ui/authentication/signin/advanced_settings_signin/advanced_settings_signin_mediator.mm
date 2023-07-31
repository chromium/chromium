// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_mediator.h"

#import "base/metrics/user_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/sync/service/sync_service.h"
#import "components/unified_consent/unified_consent_metrics.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using unified_consent::metrics::RecordSyncSetupDataTypesHistrogam;

@interface AdvancedSettingsSigninMediator ()

@property(nonatomic, assign, readonly)
    AuthenticationService* authenticationService;
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;
// Browser state preference service.
@property(nonatomic, assign, readonly) PrefService* prefService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;

@end

@implementation AdvancedSettingsSigninMediator

#pragma mark - Public

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                      syncService:(syncer::SyncService*)syncService
                      prefService:(PrefService*)prefService
                  identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    DCHECK(authenticationService);
    DCHECK(syncService);
    DCHECK(prefService);
    DCHECK(identityManager);
    _authenticationService = authenticationService;
    _syncService = syncService;
    _prefService = prefService;
    _identityManager = identityManager;
  }
  return self;
}

- (void)saveUserPreferenceForSigninResult:(SigninCoordinatorResult)signinResult
                      originalSigninState:
                          (IdentitySigninState)originalSigninState {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      RecordAction(
          UserMetricsAction("Signin_Signin_ConfirmAdvancedSyncSettings"));
      RecordSyncSetupDataTypesHistrogam(self.syncService->GetUserSettings(),
                                        self.prefService);
      break;
    }
    case SigninCoordinatorResultCanceledByUser:
      // Canceling from the advanced sync settings view is not possible.
      NOTREACHED();
      break;
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted:
      RecordAction(
          UserMetricsAction("Signin_Signin_AbortAdvancedSyncSettings"));
      [self revertToSigninState:originalSigninState];
      break;
  }
}

// For users that cancel the sign-in flow, revert to the sign-in
// state prior to starting sign-in coordinators.
- (void)revertToSigninState:(IdentitySigninState)originalState {
  switch (originalState) {
    case IdentitySigninStateSignedOut: {
      self.authenticationService->SignOut(
          signin_metrics::ProfileSignout::kAbortSignin,
          /*force_clear_browsing_data=*/false, nil);
      break;
    }
    case IdentitySigninStateSignedInWithSyncDisabled: {
      // Sync consent is not granted in Advanced Settings, therefore
      // there should be no syncing identity.
      DCHECK(!self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSync));
      break;
    }
    case IdentitySigninStateSignedInWithSyncEnabled: {
      // Switching accounts is not possible without sign-out.
      NOTREACHED();
      break;
    }
  }
}

@end

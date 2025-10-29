// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator.h"

#import "base/not_fatal_until.h"
#import "components/prefs/pref_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/cross_device_pref_tracker_factory.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator_delegate.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator_delegate.h"

@interface SyncedSetUpCoordinator () <SyncedSetUpMediatorDelegate>
@end

@implementation SyncedSetUpCoordinator {
  // Parameters relevant to understanding the app startup, used to determine
  // how the Synced Set Up flow should be presented.
  AppStartupParameters* _startupParameters;
  // Mediator for Synced Set Up. Used to determine and apply a set of synced
  // prefs to the local device.
  SyncedSetUpMediator* _syncedSetUpMediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                         startupParameters:
                             (AppStartupParameters*)startupParameters {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    CHECK(startupParameters, base::NotFatalUntil::M147);
    _startupParameters = startupParameters;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Create the `SyncedSetUpMediator` to query and apply synced prefs.
  ProfileIOS* profile = self.profile;
  sync_preferences::CrossDevicePrefTracker* tracker =
      CrossDevicePrefTrackerFactory::GetForProfile(profile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  syncer::DeviceInfoSyncService* deviceInfoSyncService =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);

  _syncedSetUpMediator =
      [[SyncedSetUpMediator alloc] initWithPrefTracker:tracker
                                 authenticationService:authService
                                 accountManagerService:accountManagerService
                                 deviceInfoSyncService:deviceInfoSyncService
                                    profilePrefService:profile->GetPrefs()
                                     startupParameters:_startupParameters
                                       identityManager:identityManager];
  _syncedSetUpMediator.delegate = self;
}

- (void)stop {
  _syncedSetUpMediator.delegate = nil;
  _syncedSetUpMediator.consumer = nil;
  [_syncedSetUpMediator disconnect];
  _syncedSetUpMediator = nil;
  _startupParameters = nil;
}

#pragma mark - SyncedSetUpMediatorDelegate

- (void)syncedSetUpMediatorDidComplete:(SyncedSetUpMediator*)mediator {
  CHECK_EQ(_syncedSetUpMediator, mediator);
  [self.delegate syncedSetUpCoordinatorWantsToBeDismissed:self];
}

@end

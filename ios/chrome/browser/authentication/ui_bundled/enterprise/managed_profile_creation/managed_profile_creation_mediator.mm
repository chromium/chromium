// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_mediator.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_consumer.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

@interface ManagedProfileCreationMediator () <
    IdentityManagerObserverBridgeDelegate> {
  BOOL _mergeBrowsingDataByDefault;
  BOOL _canShowBrowsingDataMigration;
  BOOL _browsingDataMigrationDisabledByPolicy;
  GaiaId _gaiaID;
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;

  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}
@end

@implementation ManagedProfileCreationMediator

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                    accountManagerService:
                        (ChromeAccountManagerService*)accountManagerService
                skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
               mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
    browsingDataMigrationDisabledByPolicy:
        (BOOL)browsingDataMigrationDisabledByPolicy
                                   gaiaID:(const GaiaId&)gaiaID {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _gaiaID = gaiaID;

    // We can merge if either
    // * we are at FRE,
    // * separate profiles are not supported, or
    // * the user is signed-in.
    _canShowBrowsingDataMigration =
        !skipBrowsingDataMigration &&
        AreSeparateProfilesForManagedAccountsEnabled() &&
        !identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
    _mergeBrowsingDataByDefault = mergeBrowsingDataByDefault;
    _browsingDataSeparate = !mergeBrowsingDataByDefault;
    _browsingDataMigrationDisabledByPolicy =
        browsingDataMigrationDisabledByPolicy;
  }
  return self;
}

- (void)disconnect {
  _consumer = nil;
  _delegate = nil;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _identityManagerObserver.reset();
}

- (void)setKeepBrowsingDataSeparate:(BOOL)keepSeparate {
  _browsingDataSeparate = keepSeparate;
  [self.consumer setKeepBrowsingDataSeparate:keepSeparate];
}

- (void)setConsumer:(id<ManagedProfileCreationConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  _consumer.mergeBrowsingDataByDefault = _mergeBrowsingDataByDefault;
  _consumer.canShowBrowsingDataMigration = _canShowBrowsingDataMigration;
  _consumer.browsingDataMigrationDisabledByPolicy =
      _browsingDataMigrationDisabledByPolicy;
  [_consumer setKeepBrowsingDataSeparate:self.browsingDataSeparate];
}

#pragma mark - BrowsingDataMigrationViewControllerDelegate

- (void)updateShouldKeepBrowsingDataSeparate:(BOOL)browsingDataSeparate {
  self.browsingDataSeparate = browsingDataSeparate;
  [self.consumer setKeepBrowsingDataSeparate:self.browsingDataSeparate];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onAccountsOnDeviceChanged {
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(GaiaId(_gaiaID));
  if (!identity) {
    [self.delegate identityRemovedFromDevice];
  }
}

@end

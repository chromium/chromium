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
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"

@interface ManagedProfileCreationMediator () <
    ChromeAccountManagerServiceObserver,
    IdentityManagerObserverBridgeDelegate> {
  BOOL _canShowBrowsingDataMigration;
  BOOL _browsingDataMigrationDisabledByPolicy;
  NSString* _gaiaID;
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Chrome account manager service observer bridge.
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;

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
                                   gaiaID:(NSString*)gaiaID {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
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
    _keepBrowsingDataSeparate = !mergeBrowsingDataByDefault;
    _browsingDataMigrationDisabledByPolicy =
        browsingDataMigrationDisabledByPolicy;
  }
  return self;
}

- (void)disconnect {
  _consumer = nil;
  _delegate = nil;
  _accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
  _identityManager = nullptr;
  _identityManagerObserver.reset();
}

- (void)setKeepBrowsingDataSeparate:(BOOL)keepSeparate {
  _keepBrowsingDataSeparate = keepSeparate;
  [self.consumer setKeepBrowsingDataSeparate:keepSeparate];
}

- (void)setConsumer:(id<ManagedProfileCreationConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  _consumer.canShowBrowsingDataMigration = _canShowBrowsingDataMigration;
  _consumer.browsingDataMigrationDisabledByPolicy =
      _browsingDataMigrationDisabledByPolicy;
  [_consumer setKeepBrowsingDataSeparate:self.keepBrowsingDataSeparate];
}

#pragma mark - BrowsingDataMigrationViewControllerDelegate

- (void)updateShouldKeepBrowsingDataSeparate:(BOOL)keepBrowsingDataSeparate {
  self.keepBrowsingDataSeparate = keepBrowsingDataSeparate;
  [self.consumer setKeepBrowsingDataSeparate:self.keepBrowsingDataSeparate];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `onAccountsOnDeviceChanged` instead.
    return;
  }
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(GaiaId(_gaiaID));
  if (!identity) {
    [self.delegate identityRemovedFromDevice];
  }
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  [self disconnect];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onAccountsOnDeviceChanged {
  if (!IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `identityListChanged` instead.
    return;
  }
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(GaiaId(_gaiaID));
  if (!identity) {
    [self.delegate identityRemovedFromDevice];
  }
}

@end

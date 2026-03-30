// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_mediator.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_consumer.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

@interface ManagedProfileCreationMediator () <
    IdentityManagerObserverBridgeDelegate> {
  // How to present the view.
  signin::ManagedAccountSigninMode _mode;
  GaiaId _gaiaID;
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;

  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}
@end

@implementation ManagedProfileCreationMediator

@synthesize mode = _mode;

- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
                       mode:(signin::ManagedAccountSigninMode)mode
                     gaiaID:(const GaiaId&)gaiaID {
  self = [super init];
  if (self) {
    _mode = mode;
    CHECK(accountManagerService, base::NotFatalUntil::M155);
    CHECK(identityManager, base::NotFatalUntil::M155);
    _accountManagerService = accountManagerService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _gaiaID = gaiaID;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_accountManagerService, base::NotFatalUntil::M155);
}

- (void)disconnect {
  _consumer = nil;
  _delegate = nil;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _identityManagerObserver.reset();
}

#pragma mark - ManagedProfileCreationDataSource

- (signin::ManagedAccountSigninMode)mode {
  return _mode;
}

#pragma mark - BrowsingDataMigrationViewControllerDelegate

- (void)updateShouldKeepBrowsingDataSeparate:(BOOL)browsingDataSeparate {
  switch (_mode) {
    case signin::ManagedAccountSigninMode::kForceSeparateProfileDataByPolicy:
    case signin::ManagedAccountSigninMode::kMustSeparateBecauseSignedIn:
    case signin::ManagedAccountSigninMode::kAutoMergeDuringFRE:
    case signin::ManagedAccountSigninMode::kInformOfForcedMigration:
      // The user should not have been presented with the option to make a
      // choice.
      NOTREACHED();
    case signin::ManagedAccountSigninMode::kSeparateProfileData:
      if (browsingDataSeparate) {
        return;
      }
      _mode = signin::ManagedAccountSigninMode::kMergeProfileData;
      break;
    case signin::ManagedAccountSigninMode::kMergeProfileData:
      if (!browsingDataSeparate) {
        return;
      }
      _mode = signin::ManagedAccountSigninMode::kSeparateProfileData;
  }
  [self.consumer updateUI];
}

#pragma mark - BrowsingDataMigrationViewControllerMutator

- (void)onAccountsOnDeviceChanged {
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(GaiaId(_gaiaID));
  if (!identity) {
    [self.delegate identityRemovedFromDevice];
  }
}

@end

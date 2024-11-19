// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_mediator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

@interface AccountPickerConfirmationScreenMediator () <
    ChromeAccountManagerServiceObserver,
    IdentityManagerObserverBridgeDelegate> {
}

@end

@implementation AccountPickerConfirmationScreenMediator {
  // Account manager service with observer.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Identity manager.
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Account picker configuration.
  __strong AccountPickerConfiguration* _configuration;
  // Avatar of selected identity.
  __strong UIImage* _avatar;
}

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
                    configuration:(AccountPickerConfiguration*)configuration {
  if ((self = [super init])) {
    CHECK(accountManagerService);
    CHECK(identityManager);
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _configuration = configuration;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerService);
  DCHECK(!_identityManager);
}

- (void)disconnect {
  _identityManager = nullptr;
  _identityManagerObserver.reset();
  _accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<AccountPickerConfirmationScreenConsumer>)consumer {
  _consumer = consumer;
  [self selectSelectedIdentity];
}

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  if ([_selectedIdentity isEqual:identity]) {
    return;
  }
  _selectedIdentity = identity;
  [self updateSelectedIdentityUI];
}

#pragma mark - Private

// Updates the default identity, or hide the default identity if there isn't
// one present on the device.
- (void)selectSelectedIdentity {
  if (!_accountManagerService || !_identityManager) {
    return;
  }

  id<SystemIdentity> identity = signin::GetDefaultIdentityOnDevice(
      _identityManager, _accountManagerService);

  // If the user is signed-in, present the signed-in account.
  if (_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    const CoreAccountInfo primaryAccountInfo =
        _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    id<SystemIdentity> primaryAccount =
        _accountManagerService->GetIdentityWithGaiaID(primaryAccountInfo.gaia);
    identity = primaryAccount;
  }

  // Here, default identity may be nil.
  self.selectedIdentity = identity;
}

// Updates the view controller using the default identity, or hide the default
// identity button if no identity is present on device.
- (void)updateSelectedIdentityUI {
  if (!_selectedIdentity) {
    [_consumer hideDefaultAccount];
    return;
  }

  id<SystemIdentity> selectedIdentity = _selectedIdentity;
  UIImage* avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
      selectedIdentity, IdentityAvatarSize::TableViewIcon);
  [_consumer showDefaultAccountWithFullName:selectedIdentity.userFullName
                                  givenName:selectedIdentity.userGivenName
                                      email:selectedIdentity.userEmail
                                     avatar:avatar];
}

- (void)handleIdentityListChanged {
  [self selectSelectedIdentity];
}

- (void)handleIdentityUpdated:(id<SystemIdentity>)identity {
  if ([_selectedIdentity isEqual:identity]) {
    [self updateSelectedIdentityUI];
  }
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `onAccountsOnDeviceChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `onExtendedAccountInfoUpdated` instead.
    return;
  }
  [self handleIdentityUpdated:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

#pragma mark -  IdentityManagerObserver

- (void)onAccountsOnDeviceChanged {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `identityListChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `identityUpdated` instead.
    return;
  }
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(info.gaia);
  [self handleIdentityUpdated:identity];
}

@end

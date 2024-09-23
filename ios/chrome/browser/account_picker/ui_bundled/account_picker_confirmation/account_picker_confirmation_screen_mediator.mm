// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_mediator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_consumer.h"

@interface AccountPickerConfirmationScreenMediator () <
    ChromeAccountManagerServiceObserver> {
}

@end

@implementation AccountPickerConfirmationScreenMediator {
  // Account manager service with observer.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Identity manager.
  raw_ptr<signin::IdentityManager> _identityManager;
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
    DCHECK(accountManagerService);
    DCHECK(identityManager);
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _identityManager = identityManager;
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
  if (!_accountManagerService) {
    return;
  }

  id<SystemIdentity> identity = _accountManagerService->GetDefaultIdentity();

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

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self selectSelectedIdentity];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if ([_selectedIdentity isEqual:identity]) {
    [self updateSelectedIdentityUI];
  }
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

@end

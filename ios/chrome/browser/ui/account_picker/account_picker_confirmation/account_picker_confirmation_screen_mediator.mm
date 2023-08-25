// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_mediator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_configuration.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_consumer.h"

@interface AccountPickerConfirmationScreenMediator () <
    ChromeAccountManagerServiceObserver> {
}

@end

@implementation AccountPickerConfirmationScreenMediator {
  // Account manager service with observer.
  ChromeAccountManagerService* _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Account picker configuration.
  __strong AccountPickerConfiguration* _configuration;
  // Avatar of selected identity.
  __strong UIImage* _avatar;
}

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                configuration:
                                    (AccountPickerConfiguration*)configuration {
  if (self = [super init]) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _configuration = configuration;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerService);
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<AccountPickerConfirmationScreenConsumer>)consumer {
  _consumer = consumer;
  [self selectSelectedIdentity];
}

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  if (!IsConsistencyNewAccountInterfaceEnabled()) {
    DCHECK(identity);
  }
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

  if (!IsConsistencyNewAccountInterfaceEnabled() && !identity) {
    [_delegate accountPickerConfirmationScreenMediatorNoIdentities:self];
    return;
  }

  // Here, default identity may be nil.
  _selectedIdentity = identity;
}

// Updates the view controller using the default identity, or hide the default
// identity button if no identity is present on device.
- (void)updateSelectedIdentityUI {
  if (!IsConsistencyNewAccountInterfaceEnabled()) {
    DCHECK(_selectedIdentity);
  }

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

@end

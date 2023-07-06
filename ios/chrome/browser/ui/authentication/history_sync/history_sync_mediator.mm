// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistorySyncMediator () <IdentityManagerObserverBridgeDelegate>
@end

@implementation HistorySyncMediator {
  AuthenticationService* _authenticationService;
  ChromeAccountManagerService* _accountManagerService;
  // Manager for user's Google identities.
  signin::IdentityManager* _identityManager;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Consumer for this mediator
  id<HistorySyncConsumer> _consumer;
  // Delegate
  id<HistorySyncMediatorDelegate> _delegate;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
      chromeAccountManagerService:
          (ChromeAccountManagerService*)chromeAccountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
                         consumer:(id<HistorySyncConsumer>)consumer
                         delegate:(id<HistorySyncMediatorDelegate>)delegate {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _accountManagerService = chromeAccountManagerService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _delegate = delegate;
    [self setConsumer:consumer];
  }
  return self;
}

- (void)disconnect {
  _identityManagerObserver.reset();
  _authenticationService = nil;
  _accountManagerService = nil;
  _identityManager = nil;
}

- (void)dealloc {
  // TODO(crbug.com/1454777)
  DUMP_WILL_BE_CHECK(!_accountManagerService);
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    [_delegate historySyncMediatorPrimaryAccountCleared:self];
  }
}

#pragma mark - Private

- (void)setConsumer:(id<HistorySyncConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity) {
    // This can happen if identity is removed from the device while the history
    // sync screen is open. There is no point in updating the UI since the
    // dialog will be automatically closed.
    return;
  }
  [self updateAvatarImageWithIdentity:identity];
}

// Updates the avatar image for the consumer from `identity`.
- (void)updateAvatarImageWithIdentity:(id<SystemIdentity>)identity {
  UIImage* image = _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Large);
  [_consumer setPrimaryIdentityAvatarImage:image];

  NSString* accessibilityLabel = nil;
  if (identity.userFullName.length == 0) {
    accessibilityLabel = identity.userEmail;
  } else {
    accessibilityLabel = [NSString
        stringWithFormat:@"%@ %@", identity.userFullName, identity.userEmail];
  }
  [_consumer setPrimaryIdentityAvatarAccessibilityLabel:accessibilityLabel];
}

@end

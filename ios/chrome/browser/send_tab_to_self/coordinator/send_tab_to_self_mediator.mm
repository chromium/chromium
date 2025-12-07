// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator.h"

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator_delegate.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@interface SendTabToSelfMediator () <IdentityManagerObserverBridgeDelegate> {
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  raw_ptr<AuthenticationService> _authenticationService;
  id<SystemIdentity> _primaryIdentity;
  raw_ptr<signin::IdentityManager> _identityManager;
}

@end

@implementation SendTabToSelfMediator

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager {
  if ((self = [super init])) {
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _primaryIdentity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
  }
  return self;
}

- (void)dealloc {
  CHECK(!_authenticationService, base::NotFatalUntil::M150);
  CHECK(!_identityManagerObserver, base::NotFatalUntil::M150);
}

#pragma mark - Public

- (void)disconnect {
  _authenticationService = nullptr;
  _identityManagerObserver.reset();
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfPrimaryAccountChanges {
  id<SystemIdentity> primaryIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (primaryIdentity == _primaryIdentity) {
    // No changes, so nothing to do.
    return;
  }
  _primaryIdentity = primaryIdentity;
  if (primaryIdentity) {
    // New primary identity. Refresh the view.
    [self.delegate mediatorWantsToRefreshView:self];
    return;
  }
  // User is signed-out, nothing we can do.
  [self.delegate mediatorWantsToBeStopped:self];
}

@end

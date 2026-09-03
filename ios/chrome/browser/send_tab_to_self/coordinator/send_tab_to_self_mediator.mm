// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator.h"

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator_delegate.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@interface SendTabToSelfMediator () <IdentityManagerObserving>
@end

@implementation SendTabToSelfMediator {
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  raw_ptr<AuthenticationService> _authenticationService;
  id<SystemIdentity> _primaryIdentity;
  raw_ptr<signin::IdentityManager> _identityManager;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager {
  if ((self = [super init])) {
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _primaryIdentity = _authenticationService->GetPrimaryIdentity();
  }
  return self;
}

- (void)dealloc {
  CHECK(!_authenticationService);
  CHECK(!_identityManagerObserver);
}

#pragma mark - Public

- (void)disconnect {
  _authenticationService = nullptr;
  _identityManager = nullptr;
  _identityManagerObserver.reset();
}

#pragma mark - IdentityManagerObserving

- (void)batchOfPrimaryAccountChangesDidEnd {
  id<SystemIdentity> primaryIdentity =
      _authenticationService->GetPrimaryIdentity();
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

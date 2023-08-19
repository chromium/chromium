// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_mediator.h"

#import "base/check.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"

@interface UnifiedConsentMediator () <ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

// Unified consent view controller.
@property(nonatomic, weak)
    UnifiedConsentViewController* unifiedConsentViewController;
// NO until the mediator is started.
@property(nonatomic, assign) BOOL started;
// Authentication service for identities.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation UnifiedConsentMediator

- (instancetype)initWithUnifiedConsentViewController:
                    (UnifiedConsentViewController*)viewController
                               authenticationService:
                                   (AuthenticationService*)authenticationService
                               accountManagerService:
                                   (ChromeAccountManagerService*)
                                       accountManagerService {
  self = [super init];
  if (self) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
    _unifiedConsentViewController = viewController;
    _authenticationService = authenticationService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
}

- (void)start {
  DCHECK(self.accountManagerService);

  if (!self.selectedIdentity) {
    // Select an identity if not selected yet.
    self.selectedIdentity = [self findDefaultSelectedIdentity];
  }

  // Make sure the view is loaded so the mediator can set it up.
  [self.unifiedConsentViewController loadViewIfNeeded];
  self.started = YES;
  [self updateViewController];
}

- (void)disconnect {
  self.accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if ([_selectedIdentity isEqual:selectedIdentity]) {
    return;
  }
  // nil is allowed only if there is no other identity.
  DCHECK(selectedIdentity || !self.accountManagerService->HasIdentities());
  _selectedIdentity = selectedIdentity;
  [self updateViewController];
}

#pragma mark - Private

- (id<SystemIdentity>)findDefaultSelectedIdentity {
  if (self.authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    return self.authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
  }

  return self.accountManagerService->GetDefaultIdentity();
}

// Updates the view if the mediator has been started.
- (void)updateViewController {
  // The UI should not be updated before the view is loaded.
  if (!self.started)
    return;
  if (!self.accountManagerService)
    return;

  id<SystemIdentity> selectedIdentity = self.selectedIdentity;
  if (selectedIdentity) {
    [self.unifiedConsentViewController
        updateIdentityButtonControlWithUserFullName:selectedIdentity
                                                        .userFullName
                                              email:selectedIdentity.userEmail];
    UIImage* avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
        selectedIdentity, IdentityAvatarSize::Regular);
    DCHECK(avatar);
    [self.unifiedConsentViewController
        updateIdentityButtonControlWithAvatar:avatar];
  } else {
    [self.unifiedConsentViewController hideIdentityButtonControl];
  }
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (!self.accountManagerService)
    return;

  if (!self.accountManagerService->IsValidIdentity(self.selectedIdentity)) {
    self.selectedIdentity = [self findDefaultSelectedIdentity];
    [self.delegate
        unifiedConsentViewMediatorDelegateNeedPrimaryButtonUpdate:self];
  }
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateViewController];
  }
}

@end

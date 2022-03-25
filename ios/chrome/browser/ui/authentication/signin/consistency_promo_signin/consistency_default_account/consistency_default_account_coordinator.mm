// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_view_controller.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyDefaultAccountCoordinator () <
    ConsistencyDefaultAccountActionDelegate,
    ConsistencyDefaultAccountMediatorDelegate>

@property(nonatomic, strong)
    ConsistencyDefaultAccountViewController* defaultAccountViewController;

@property(nonatomic, strong) ConsistencyDefaultAccountMediator* mediator;

@end

@implementation ConsistencyDefaultAccountCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.mediator = [[ConsistencyDefaultAccountMediator alloc]
      initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForBrowserState(browserState)];
  self.mediator.delegate = self;
  self.defaultAccountViewController =
      [[ConsistencyDefaultAccountViewController alloc] init];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  PrefService* prefService = browserState->GetPrefs();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  self.defaultAccountViewController.enterpriseSignInRestrictions =
      GetEnterpriseSignInRestrictions(authenticationService, prefService,
                                      syncService);
  self.mediator.consumer = self.defaultAccountViewController;
  self.defaultAccountViewController.actionDelegate = self;
  self.defaultAccountViewController.layoutDelegate = self.layoutDelegate;
  [self.defaultAccountViewController view];
}

- (void)startSigninSpinner {
  [self.defaultAccountViewController startSpinner];
}

- (void)stopSigninSpinner {
  [self.defaultAccountViewController stopSpinner];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.defaultAccountViewController;
}

- (ChromeIdentity*)selectedIdentity {
  return self.mediator.selectedIdentity;
}

- (void)setSelectedIdentity:(ChromeIdentity*)identity {
  DCHECK(self.mediator);
  self.mediator.selectedIdentity = identity;
}

#pragma mark - ConsistencyDefaultAccountMediatorDelegate

- (void)consistencyDefaultAccountMediatorNoIdentities:
    (ConsistencyDefaultAccountMediator*)mediator {
  [self.delegate consistencyDefaultAccountCoordinatorAllIdentityRemoved:self];
}

#pragma mark - ConsistencyDefaultAccountActionDelegate

- (void)consistencyDefaultAccountViewControllerSkip:
    (ConsistencyDefaultAccountViewController*)viewController {
  [self.delegate consistencyDefaultAccountCoordinatorSkip:self];
}

- (void)consistencyDefaultAccountViewControllerOpenIdentityChooser:
    (ConsistencyDefaultAccountViewController*)viewController {
  [self.delegate consistencyDefaultAccountCoordinatorOpenIdentityChooser:self];
}

- (void)consistencyDefaultAccountViewControllerContinueWithSelectedIdentity:
    (ConsistencyDefaultAccountViewController*)viewController {
  [self.delegate consistencyDefaultAccountCoordinatorSignin:self];
}

@end

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_coordinator.h"

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

@interface ConsistencyDefaultAccountCoordinator () <
    ConsistencyDefaultAccountActionDelegate>

@property(nonatomic, strong)
    ConsistencyDefaultAccountViewController* defaultAccountViewController;

@property(nonatomic, strong) ConsistencyDefaultAccountMediator* mediator;

@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

@end

@implementation ConsistencyDefaultAccountCoordinator {
  // Used to customize content on screen.
  SigninContextStyle _contextStyle;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _contextStyle = contextStyle;
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)dealloc {
  CHECK(!self.mediator, base::NotFatalUntil::M142);
  CHECK(!self.defaultAccountViewController, base::NotFatalUntil::M142);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  ProfileIOS* profile = self.profile;
  self.mediator = [[ConsistencyDefaultAccountMediator alloc]
      initWithIdentityManager:IdentityManagerFactory::GetForProfile(profile)
        accountManagerService:ChromeAccountManagerServiceFactory::GetForProfile(
                                  profile)
                  syncService:SyncServiceFactory::GetForProfile(profile)
                 contextStyle:_contextStyle
                  accessPoint:self.accessPoint];
  self.defaultAccountViewController =
      [[ConsistencyDefaultAccountViewController alloc] init];
  self.mediator.consumer = self.defaultAccountViewController;
  self.defaultAccountViewController.actionDelegate = self;
  self.defaultAccountViewController.layoutDelegate = self.layoutDelegate;
  [self.defaultAccountViewController view];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  self.defaultAccountViewController = nil;
  [super stop];
}

#pragma mark - Public

- (void)startSigninSpinner {
  [self.defaultAccountViewController startSpinner];
}

- (void)stopSigninSpinner {
  [self.defaultAccountViewController stopSpinner];
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.defaultAccountViewController;
}

- (id<SystemIdentity>)selectedIdentity {
  return self.mediator.selectedIdentity;
}

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  DCHECK(self.mediator);
  self.mediator.selectedIdentity = identity;
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

- (void)consistencyDefaultAccountViewControllerAddAccountAndSignin:
    (ConsistencyDefaultAccountViewController*)viewController {
  [self.delegate consistencyDefaultAccountCoordinatorOpenAddAccount:self];
}

@end

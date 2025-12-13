// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/safe_browsing/safe_browsing_standard_protection_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/safe_browsing/safe_browsing_standard_protection_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/safe_browsing/safe_browsing_standard_protection_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface SafeBrowsingStandardProtectionCoordinator () <
    SafeBrowsingStandardProtectionViewControllerPresentationDelegate>

// View controller handled by coordinator.
@property(nonatomic, strong)
    SafeBrowsingStandardProtectionViewController* viewController;
// Mediator handled by coordinator.
@property(nonatomic, strong) SafeBrowsingStandardProtectionMediator* mediator;

@end

@implementation SafeBrowsingStandardProtectionCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewController = [[SafeBrowsingStandardProtectionViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator = [[SafeBrowsingStandardProtectionMediator alloc]
      initWithUserPrefService:self.profile->GetPrefs()
                  authService:AuthenticationServiceFactory::GetForProfile(
                                  self.profile)
              identityManager:IdentityManagerFactory::GetForProfile(
                                  self.profile)];
  self.mediator.consumer = self.viewController;
  DCHECK(self.baseNavigationController);
  [self.baseNavigationController
      presentViewController:self.viewController.navigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self.mediator disconnect];
}

#pragma mark - SafeBrowsingStandardProtectionViewControllerPresentationDelegate

- (void)safeBrowsingStandardProtectionViewControllerDidRemove:
    (SafeBrowsingStandardProtectionViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate safeBrowsingStandardProtectionCoordinatorDidRemove:self];
}

@end

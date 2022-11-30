// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_coordinator.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_mediator.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewController = [[SafeBrowsingStandardProtectionViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator = [[SafeBrowsingStandardProtectionMediator alloc]
      initWithUserPrefService:self.browser->GetBrowserState()->GetPrefs()
                  authService:AuthenticationServiceFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())
              identityManager:IdentityManagerFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())];
  self.mediator.consumer = self.viewController;
  self.viewController.modelDelegate = self.mediator;
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

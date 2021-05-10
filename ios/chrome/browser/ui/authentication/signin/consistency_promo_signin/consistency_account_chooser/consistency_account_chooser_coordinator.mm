// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyAccountChooserCoordinator () <
    ConsistencyAccountChooserTableViewControllerActionDelegate>

@property(nonatomic, strong)
    ConsistencyAccountChooserViewController* accountChooserViewController;

@property(nonatomic, strong) ConsistencyAccountChooserMediator* mediator;
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;

@end

@implementation ConsistencyAccountChooserCoordinator

- (void)startWithSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  [super start];
  self.mediator = [[ConsistencyAccountChooserMediator alloc]
      initWithSelectedIdentity:selectedIdentity
                   prefService:self.browser->GetBrowserState()->GetPrefs()];

  self.accountChooserViewController =
      [[ConsistencyAccountChooserViewController alloc] init];
  self.accountChooserViewController.modelDelegate = self.mediator;
  self.mediator.consumer = self.accountChooserViewController.consumer;
  self.accountChooserViewController.actionDelegate = self;
  [self.accountChooserViewController view];
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.accountChooserViewController;
}

- (ChromeIdentity*)selectedIdentity {
  return self.mediator.selectedIdentity;
}

#pragma mark - ConsistencyAccountChooserTableViewControllerPresentationDelegate

- (void)consistencyAccountChooserTableViewController:
            (ConsistencyAccountChooserTableViewController*)viewController
                         didSelectIdentityWithGaiaID:(NSString*)gaiaID {
  ios::ChromeIdentityService* identityService =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  ChromeIdentity* identity =
      identityService->GetIdentityWithGaiaID(base::SysNSStringToUTF8(gaiaID));
  DCHECK(identity);
  self.mediator.selectedIdentity = identity;
  [self.delegate
      consistencyAccountChooserCoordinatorChromeIdentitySelected:self];
}

- (void)consistencyAccountChooserTableViewControllerDidTapOnAddAccount:
    (ConsistencyAccountChooserTableViewController*)viewController {
  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_WEB_SIGNIN];
  __weak __typeof(self) weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        [weakSelf.addAccountSigninCoordinator stop];
        weakSelf.addAccountSigninCoordinator = nil;
      };
  [self.addAccountSigninCoordinator start];
}

@end

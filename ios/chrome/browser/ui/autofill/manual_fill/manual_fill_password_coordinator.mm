// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"

#import "base/apple/foundation_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ui/base/device_form_factor.h"

@interface ManualFillPasswordCoordinator () <PasswordListNavigator>

// Fetches and filters the passwords for the view controller.
@property(nonatomic, strong) ManualFillPasswordMediator* passwordMediator;

// The view controller presented above the keyboard where the user can select
// one of their passwords.
@property(nonatomic, strong) PasswordViewController* passwordViewController;

// Button presenting this coordinator in a popover. Used for continuation after
// dismissing any presented view controller. iPad only.
@property(nonatomic, weak) UIButton* presentingButton;

@end

@implementation ManualFillPasswordCoordinator

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type (i.e. PasswordCoordinatorDelegate
// extends FallbackCoordinatorDelegate)
@dynamic delegate;

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                           URL:(const GURL&)URL
              injectionHandler:(ManualFillInjectionHandler*)injectionHandler
      invokedOnObfuscatedField:(BOOL)invokedOnObfuscatedField
                        formID:(const autofill::FormRendererId)formID
                       frameID:(const std::string&)frameID {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                          injectionHandler:injectionHandler];
  if (self) {
    _passwordViewController =
        [[PasswordViewController alloc] initWithSearchController:nil];

    ChromeBrowserState* browserState = self.browser->GetBrowserState();
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(browserState);
    auto profilePasswordStore =
        IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
            browserState, ServiceAccessType::EXPLICIT_ACCESS);
    auto accountPasswordStore =
        IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
            browserState, ServiceAccessType::EXPLICIT_ACCESS);

    _passwordMediator = [[ManualFillPasswordMediator alloc]
           initWithFaviconLoader:faviconLoader
                        webState:browser->GetWebStateList()->GetActiveWebState()
                     syncService:syncService
                             URL:URL
        invokedOnObfuscatedField:invokedOnObfuscatedField
            profilePasswordStore:profilePasswordStore
            accountPasswordStore:accountPasswordStore];
    [_passwordMediator fetchPasswordsForForm:formID frame:frameID];
    _passwordMediator.actionSectionEnabled = YES;
    _passwordMediator.consumer = _passwordViewController;
    _passwordMediator.navigator = self;
    _passwordMediator.contentInjector = injectionHandler;

    _passwordViewController.imageDataSource = _passwordMediator;
  }
  return self;
}

- (void)stop {
  [super stop];
  [self.activeChildCoordinator stop];
  [self.childCoordinators removeAllObjects];

  [_passwordMediator disconnect];
  _passwordMediator.consumer = nil;
  _passwordMediator = nil;
}

- (void)presentFromButton:(UIButton*)button {
  [super presentFromButton:button];
  self.presentingButton = button;
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.passwordViewController;
}

#pragma mark - PasswordListNavigator

- (void)openAllPasswordsList {
  __weak id<PasswordCoordinatorDelegate> weakDelegate = self.delegate;

  [self dismissIfNecessaryThenDoCompletion:^{
    [weakDelegate openAllPasswordsPicker];
  }];
}

- (void)openPasswordManager {
  __weak id<PasswordCoordinatorDelegate> weakDelegate = self.delegate;

  [self dismissIfNecessaryThenDoCompletion:^{
    [weakDelegate openPasswordManager];
  }];
}

- (void)openPasswordSettings {
  __weak id<PasswordCoordinatorDelegate> weakDelegate = self.delegate;

  [self dismissIfNecessaryThenDoCompletion:^{
    [weakDelegate openPasswordSettings];
  }];
}

- (void)openPasswordSuggestion {
  __weak id<PasswordCoordinatorDelegate> weakDelegate = self.delegate;

  [self dismissIfNecessaryThenDoCompletion:^{
    [weakDelegate openPasswordSuggestion];
  }];
}

@end

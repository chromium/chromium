// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"

#import "base/mac/foundation_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       URL:(const GURL&)URL
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler
                    invokedOnPasswordField:(BOOL)invokedOnPasswordField {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                          injectionHandler:injectionHandler];
  if (self) {
    _passwordViewController =
        [[PasswordViewController alloc] initWithSearchController:nil];

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browser->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS);
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(
            browser->GetBrowserState());
    SyncSetupService* syncService = SyncSetupServiceFactory::GetForBrowserState(
        self.browser->GetBrowserState());

    _passwordMediator = [[ManualFillPasswordMediator alloc]
         initWithPasswordStore:passwordStore
                 faviconLoader:faviconLoader
                      webState:browser->GetWebStateList()->GetActiveWebState()
                   syncService:syncService
                           URL:URL
        invokedOnPasswordField:invokedOnPasswordField];
    [_passwordMediator fetchPasswords];
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

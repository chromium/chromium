// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"

#include "base/mac/foundation_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

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
                              (ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                          injectionHandler:injectionHandler];
  if (self) {
    _passwordViewController =
        [[PasswordViewController alloc] initWithSearchController:nil];
    _passwordViewController.contentInsetsAlwaysEqualToSafeArea = YES;

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browser->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS);
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(
            browser->GetBrowserState());

    _passwordMediator = [[ManualFillPasswordMediator alloc]
        initWithPasswordStore:passwordStore
                faviconLoader:faviconLoader];
    [_passwordMediator fetchPasswordsForURL:URL];
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
  // On iPad, first dismiss the popover before the new view is presented.
  __weak __typeof(self) weakSelf = self;
  if (IsIPadIdiom() && self.passwordViewController.presentingViewController) {
    [self.passwordViewController
        dismissViewControllerAnimated:true
                           completion:^{
                             [weakSelf openAllPasswordsList];
                           }];
    return;
  }
  [self.delegate openAllPasswordsPicker];
}

- (void)openPasswordSettings {
  __weak id<PasswordCoordinatorDelegate> delegate = self.delegate;
  [self dismissIfNecessaryThenDoCompletion:^{
    [delegate openPasswordSettings];
    if (IsIPadIdiom()) {
      // Settings close the popover but don't send a message to reopen it.
      [delegate fallbackCoordinatorDidDismissPopover:self];
    }
  }];
}

@end

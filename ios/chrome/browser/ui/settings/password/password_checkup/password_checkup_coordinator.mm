// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordCheckupCoordinator () <PasswordCheckupCommands>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordCheckupViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordCheckupMediator* mediator;

@end

@implementation PasswordCheckupCoordinator

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
  [super start];
  self.viewController = [[PasswordCheckupViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.handler = self;
  self.mediator = [[PasswordCheckupMediator alloc]
      initWithPasswordCheckManager:IOSChromePasswordCheckManagerFactory::
                                       GetForBrowserState(
                                           self.browser->GetBrowserState())];
  self.viewController.delegate = self.mediator;
  self.mediator.consumer = self.viewController;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController.handler = nil;
  self.viewController = nil;
}

#pragma mark - PasswordCheckupCommands

- (void)dismissPasswordCheckupViewController {
  [self.delegate passwordCheckupCoordinatorDidRemove:self];
}

@end

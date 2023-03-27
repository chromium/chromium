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
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordCheckupCoordinator () <PasswordCheckupCommands,
                                          PasswordIssuesCoordinatorDelegate> {
  // Coordinator for password issues.
  PasswordIssuesCoordinator* _passwordIssuesCoordinator;

  // Reauthentication module used by password issues coordinator.
  ReauthenticationModule* _reauthModule;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordCheckupViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordCheckupMediator* mediator;

@end

@implementation PasswordCheckupCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                    reauthModule:
                                        (ReauthenticationModule*)reauthModule {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _reauthModule = reauthModule;
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

  [self stopPasswordIssuesCoordinator];
}

#pragma mark - PasswordCheckupCommands

- (void)dismissPasswordCheckupViewController {
  [self.delegate passwordCheckupCoordinatorDidRemove:self];
}

- (void)showPasswordIssuesWithWarningType:
    (password_manager::WarningType)warningType {
  CHECK(!_passwordIssuesCoordinator);
  _passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:warningType
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  _passwordIssuesCoordinator.delegate = self;
  _passwordIssuesCoordinator.reauthModule = _reauthModule;
  [_passwordIssuesCoordinator start];
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  CHECK_EQ(_passwordIssuesCoordinator, coordinator);
  [self stopPasswordIssuesCoordinator];
}

#pragma mark - Private

- (void)stopPasswordIssuesCoordinator {
  [_passwordIssuesCoordinator stop];
  _passwordIssuesCoordinator.delegate = nil;
  _passwordIssuesCoordinator = nil;
}

@end

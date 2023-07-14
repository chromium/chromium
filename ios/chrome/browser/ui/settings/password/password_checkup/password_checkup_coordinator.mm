// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"

#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/password_checkup_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordCheckReferrer;

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

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                        reauthModule:(ReauthenticationModule*)reauthModule
                            referrer:(PasswordCheckReferrer)referrer {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _reauthModule = reauthModule;
    _dispatcher = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                     ApplicationCommands);
    password_manager::LogPasswordCheckReferrer(referrer);
  }
  return self;
}

- (void)start {
  [super start];

  password_manager::LogOpenPasswordCheckupHomePage();
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

// TODO(crbug.com/1464966): Make sure there aren't mutiple active
// `_passwordIssuesCoordinator`s at once.
- (void)showPasswordIssuesWithWarningType:
    (password_manager::WarningType)warningType {
  password_manager::LogOpenPasswordIssuesList(warningType);

  _passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:warningType
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  _passwordIssuesCoordinator.delegate = self;
  _passwordIssuesCoordinator.reauthModule = _reauthModule;
  [_passwordIssuesCoordinator start];
}

- (void)dismissAndOpenURL:(CrURL*)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)dismissAfterAllPasswordsGone {
  NSArray<UIViewController*>* viewControllers =
      self.baseNavigationController.viewControllers;
  NSInteger viewControllerIndex =
      [viewControllers indexOfObject:self.viewController];

  // Nothing to do if the view controller was already removed from the
  // navigation stack.
  if (viewControllerIndex == NSNotFound) {
    return;
  }

  CHECK_GT(viewControllerIndex, 0);

  // Go to the previous view controller in the navigation stack.
  [self.baseNavigationController
      popToViewController:viewControllers[viewControllerIndex - 1]
                 animated:YES];
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

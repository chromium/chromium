// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"

#import "base/mac/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::WarningType;

namespace {

// Returns a DetailsContext based on the given WarningType.
DetailsContext ComputeDetailsContextFromWarningType(WarningType warning_type) {
  switch (warning_type) {
    case WarningType::kCompromisedPasswordsWarning:
      return DetailsContext::kCompromisedIssues;
    case WarningType::kReusedPasswordsWarning:
      return DetailsContext::kReusedIssues;
    case WarningType::kWeakPasswordsWarning:
      return DetailsContext::kWeakIssues;
    case WarningType::kDismissedWarningsWarning:
      return DetailsContext::kDismissedWarnings;
    case WarningType::kNoInsecurePasswordsWarning:
      return DetailsContext::kGeneral;
  }
}

}  // namespace

@interface PasswordIssuesCoordinator () <PasswordDetailsCoordinatorDelegate,
                                         PasswordIssuesCoordinatorDelegate,
                                         PasswordIssuesPresenter> {
  // Password check manager to power mediator.
  IOSChromePasswordCheckManager* _manager;

  // Type of insecure credentials issues to display.
  password_manager::WarningType _warningType;

  // Coordinator for password issues displaying dismissed compromised
  // credentials.
  PasswordIssuesCoordinator* _dismissedPasswordIssuesCoordinator;

  // Flag indicating if the coordinator should dismiss its view controller after
  // the view controller of a child coordinator is removed from the stack. When
  // the issues and dismissed warnings are removed by the user, the coordinator
  // should dismiss its view controller and go back to the previous screen. If
  // there are child coordinators, this flag is used to dismiss the view
  // controller after the children are dismissed.
  BOOL _shouldDismissAfterChildCoordinatorRemoved;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordIssuesTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordIssuesMediator* mediator;

// Coordinator for password details.
@property(nonatomic, strong) PasswordDetailsCoordinator* passwordDetails;

@end

@implementation PasswordIssuesCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initForWarningType:(password_manager::WarningType)warningType
          baseNavigationController:(UINavigationController*)navigationController
                           browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _warningType = warningType;
    _baseNavigationController = navigationController;
    _dispatcher = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                     ApplicationCommands);
  }
  return self;
}

- (void)start {
  [super start];
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.mediator = [[PasswordIssuesMediator alloc]
        initForWarningType:_warningType
      passwordCheckManager:IOSChromePasswordCheckManagerFactory::
                               GetForBrowserState(browserState)
                                   .get()
             faviconLoader:IOSChromeFaviconLoaderFactory::GetForBrowserState(
                               browserState)
               syncService:SyncServiceFactory::GetForBrowserState(
                               browserState)];

  PasswordIssuesTableViewController* passwordIssuesTableViewController =
      [[PasswordIssuesTableViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  passwordIssuesTableViewController.imageDataSource = self.mediator;
  self.viewController = passwordIssuesTableViewController;

  // If reauthentication module was not provided, coordinator will create its
  // own.
  if (!self.reauthModule) {
    self.reauthModule = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self.mediator];
  }

  self.mediator.consumer = self.viewController;
  self.viewController.presenter = self;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;

  [self.passwordDetails stop];
  self.passwordDetails.delegate = nil;
  self.passwordDetails = nil;

  [self stopDismissedPasswordIssuesCoordinator];
}

#pragma mark - PasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
  [self.delegate passwordIssuesCoordinatorDidRemove:self];
}

- (void)dismissAndOpenURL:(CrURL*)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)presentPasswordIssueDetails:(PasswordIssue*)password {
  DCHECK(!self.passwordDetails);
  self.passwordDetails = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                            credential:password.credential
                          reauthModule:self.reauthModule
                               context:ComputeDetailsContextFromWarningType(
                                           _warningType)];
  self.passwordDetails.delegate = self;
  [self.passwordDetails start];
}

- (void)presentDismissedCompromisedCredentials {
  CHECK(!_dismissedPasswordIssuesCoordinator);
  _dismissedPasswordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:password_manager::WarningType::
                                   kDismissedWarningsWarning
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  _dismissedPasswordIssuesCoordinator.reauthModule = self.reauthModule;
  _dismissedPasswordIssuesCoordinator.delegate = self;
  [_dismissedPasswordIssuesCoordinator start];
}

- (void)dismissAfterAllIssuesGone {
  if (self.baseNavigationController.topViewController == self.viewController) {
    [self.baseNavigationController popViewControllerAnimated:NO];
  } else {
    _shouldDismissAfterChildCoordinatorRemoved = YES;
  }
}

#pragma mark - PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetails, coordinator);
  [self.passwordDetails stop];
  self.passwordDetails.delegate = nil;
  self.passwordDetails = nil;

  [self onChildCoordinatorDidRemove];
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  CHECK_EQ(_dismissedPasswordIssuesCoordinator, coordinator);
  [self stopDismissedPasswordIssuesCoordinator];

  [self onChildCoordinatorDidRemove];
}

#pragma mark - Private

- (void)stopDismissedPasswordIssuesCoordinator {
  [_dismissedPasswordIssuesCoordinator stop];
  _dismissedPasswordIssuesCoordinator.reauthModule = nil;
  _dismissedPasswordIssuesCoordinator.delegate = nil;
  _dismissedPasswordIssuesCoordinator = nil;
}

// Called after the view controller of a child coordinator of `self` was removed
// from the navigation stack.
- (void)onChildCoordinatorDidRemove {
  // If the content of the view controller was gone while a child coordinator
  // was presenting content, dismiss the view controller now that the child
  // coordinator's vc was removed.
  if (_shouldDismissAfterChildCoordinatorRemoved) {
    CHECK_EQ(self.baseNavigationController.topViewController,
             self.viewController);
    _shouldDismissAfterChildCoordinatorRemoved = NO;
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.baseNavigationController popViewControllerAnimated:NO];
    });
  }
}

@end

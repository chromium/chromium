// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_coordinator.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_issue.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordIssuesCoordinator () <PasswordDetailsCoordinatorDelegate,
                                         PasswordIssuesPresenter> {
  // Password check manager to power mediator.
  IOSChromePasswordCheckManager* _manager;
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

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                            passwordCheckManager:
                                (IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _manager = manager;
    _dispatcher = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                     ApplicationCommands);
  }
  return self;
}

- (void)start {
  [super start];
  // To start, a password check manager should be ready.
  DCHECK(_manager);

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);

  self.mediator =
      [[PasswordIssuesMediator alloc] initWithPasswordCheckManager:_manager
                                                     faviconLoader:faviconLoader
                                                       syncService:syncService];

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
  self.mediator = nil;
  self.viewController = nil;

  [self.passwordDetails stop];
  self.passwordDetails.delegate = nil;
  self.passwordDetails = nil;
}

#pragma mark - PasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
  [self.delegate passwordIssuesCoordinatorDidRemove:self];
}

- (void)presentPasswordIssueDetails:(PasswordIssue*)password {
  DCHECK(!self.passwordDetails);
  self.passwordDetails = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                            credential:password.credential
                          reauthModule:self.reauthModule
                  passwordCheckManager:_manager];
  self.passwordDetails.delegate = self;
  [self.passwordDetails start];
}

#pragma mark - PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetails, coordinator);
  [self.passwordDetails stop];
  self.passwordDetails.delegate = nil;
  self.passwordDetails = nil;
}

- (void)passwordDetailsCoordinator:(PasswordDetailsCoordinator*)coordinator
                  deleteCredential:
                      (const password_manager::CredentialUIEntry&)credential {
  if (![self.delegate willHandlePasswordDeletion:credential]) {
    [self.mediator deleteCredential:credential];
  }
  [self.baseNavigationController popViewControllerAnimated:YES];
}

@end

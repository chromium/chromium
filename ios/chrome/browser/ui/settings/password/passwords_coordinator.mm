// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_settings_commands.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordsCoordinator () <
    AddPasswordCoordinatorDelegate,
    PasswordDetailsCoordinatorDelegate,
    PasswordIssuesCoordinatorDelegate,
    PasswordsInOtherAppsCoordinatorDelegate,
    PasswordsSettingsCommands,
    PasswordManagerViewControllerPresentationDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong)
    PasswordManagerViewController* passwordsViewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordsMediator* mediator;

// Reauthentication module used by passwords export and password details.
@property(nonatomic, strong) ReauthenticationModule* reauthModule;

// The dispatcher used by `viewController`.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>
        dispatcher;

// Coordinator for password details.
@property(nonatomic, strong)
    PasswordIssuesCoordinator* passwordIssuesCoordinator;

// Coordinator for editing existing password details.
@property(nonatomic, strong)
    PasswordDetailsCoordinator* passwordDetailsCoordinator;

// Coordinator for add password details.
@property(nonatomic, strong) AddPasswordCoordinator* addPasswordCoordinator;

// Coordinator for passwords in other apps promotion view.
@property(nonatomic, strong)
    PasswordsInOtherAppsCoordinator* passwordsInOtherAppsCoordinator;

@end

@implementation PasswordsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _dispatcher = static_cast<
        id<BrowserCommands, ApplicationCommands, BrowsingDataCommands>>(
        browser->GetCommandDispatcher());
  }
  return self;
}

- (void)checkSavedPasswords {
  [self.mediator startPasswordCheck];
  base::UmaHistogramEnumeration(
      "PasswordManager.BulkCheck.UserAction",
      password_manager::metrics_util::PasswordCheckInteraction::
          kAutomaticPasswordCheck);
}

- (UIViewController*)viewController {
  return self.passwordsViewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
  self.mediator = [[PasswordsMediator alloc]
      initWithPasswordCheckManager:[self passwordCheckManager]
                  syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                       browserState)
                     faviconLoader:faviconLoader
                   identityManager:IdentityManagerFactory::GetForBrowserState(
                                       browserState)
                       syncService:SyncServiceFactory::GetForBrowserState(
                                       browserState)];
  self.reauthModule = [[ReauthenticationModule alloc]
      initWithSuccessfulReauthTimeAccessor:self.mediator];

  self.passwordsViewController =
      [[PasswordManagerViewController alloc] initWithBrowser:self.browser];

  self.passwordsViewController.handler = self;
  self.passwordsViewController.delegate = self.mediator;
  self.passwordsViewController.dispatcher = self.dispatcher;
  self.passwordsViewController.presentationDelegate = self;
  self.passwordsViewController.reauthenticationModule = self.reauthModule;
  self.passwordsViewController.imageDataSource = self.mediator;

  self.mediator.consumer = self.passwordsViewController;

  [self.baseNavigationController pushViewController:self.passwordsViewController
                                           animated:YES];
}

- (void)stop {
  self.passwordsViewController.delegate = nil;
  self.passwordsViewController = nil;

  [self.passwordIssuesCoordinator stop];
  self.passwordIssuesCoordinator.delegate = nil;
  self.passwordIssuesCoordinator = nil;

  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;

  [self.passwordsInOtherAppsCoordinator stop];
  self.passwordsInOtherAppsCoordinator.delegate = nil;
  self.passwordsInOtherAppsCoordinator = nil;

  [self.mediator disconnect];
}

#pragma mark - PasswordsSettingsCommands

- (void)showCompromisedPasswords {
  DCHECK(!self.passwordIssuesCoordinator);
  self.passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                  passwordCheckManager:[self passwordCheckManager].get()];
  self.passwordIssuesCoordinator.delegate = self;
  self.passwordIssuesCoordinator.reauthModule = self.reauthModule;
  [self.passwordIssuesCoordinator start];
}

- (void)showDetailedViewForForm:(const password_manager::PasswordForm&)form {
  DCHECK(!self.passwordDetailsCoordinator);
  self.passwordDetailsCoordinator = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                              password:form
                          reauthModule:self.reauthModule
                  passwordCheckManager:[self passwordCheckManager].get()];
  self.passwordDetailsCoordinator.delegate = self;
  [self.passwordDetailsCoordinator start];
}

- (void)showAddPasswordSheet {
  DCHECK(!self.addPasswordCoordinator);
  self.addPasswordCoordinator = [[AddPasswordCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                    reauthModule:self.reauthModule
            passwordCheckManager:[self passwordCheckManager].get()];
  self.addPasswordCoordinator.delegate = self;
  [self.addPasswordCoordinator start];
}

- (void)showPasswordsInOtherAppsPromo {
  DCHECK(!self.passwordsInOtherAppsCoordinator);
  self.passwordsInOtherAppsCoordinator =
      [[PasswordsInOtherAppsCoordinator alloc]
          initWithBaseNavigationController:self.baseNavigationController
                                   browser:self.browser];
  self.passwordsInOtherAppsCoordinator.delegate = self;
  [self.passwordsInOtherAppsCoordinator start];
}

#pragma mark - PasswordManagerViewControllerPresentationDelegate

- (void)PasswordManagerViewControllerDismissed {
  [self.delegate passwordsCoordinatorDidRemove:self];
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  DCHECK_EQ(self.passwordIssuesCoordinator, coordinator);
  [self.passwordIssuesCoordinator stop];
  self.passwordIssuesCoordinator.delegate = nil;
  self.passwordIssuesCoordinator = nil;
}

- (BOOL)willHandlePasswordDeletion:
    (const password_manager::PasswordForm&)password {
  [self.mediator deletePasswordForm:password];
  return YES;
}

#pragma mark PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetailsCoordinator, coordinator);
  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;
}

- (void)passwordDetailsCoordinator:(PasswordDetailsCoordinator*)coordinator
                    deletePassword:
                        (const password_manager::PasswordForm&)password {
  DCHECK_EQ(self.passwordDetailsCoordinator, coordinator);
  [self.mediator deletePasswordForm:password];
  [self.baseNavigationController popViewControllerAnimated:YES];
}

#pragma mark AddPasswordDetailsCoordinatorDelegate

- (void)passwordDetailsTableViewControllerDidFinish:
    (AddPasswordCoordinator*)coordinator {
  DCHECK_EQ(self.addPasswordCoordinator, coordinator);
  [self.addPasswordCoordinator stop];
  self.addPasswordCoordinator.delegate = nil;
  self.addPasswordCoordinator = nil;
}

- (void)setMostRecentlyUpdatedPasswordDetails:
    (const password_manager::PasswordForm&)password {
  [self.passwordsViewController setMostRecentlyUpdatedPasswordDetails:password];
}

- (void)dismissAddViewControllerAndShowPasswordDetails:
            (const password_manager::PasswordForm&)password
                                           coordinator:(AddPasswordCoordinator*)
                                                           coordinator {
  DCHECK(self.addPasswordCoordinator &&
         self.addPasswordCoordinator == coordinator);
  [self passwordDetailsTableViewControllerDidFinish:coordinator];
  [self showDetailedViewForForm:password];
  [self.passwordDetailsCoordinator
          showPasswordDetailsInEditModeWithoutAuthentication];
}

#pragma mark - PasswordsInOtherAppsCoordinatorDelegate

- (void)passwordsInOtherAppsCoordinatorDidRemove:
    (PasswordsInOtherAppsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordsInOtherAppsCoordinator, coordinator);
  [self.passwordsInOtherAppsCoordinator stop];
  self.passwordsInOtherAppsCoordinator.delegate = nil;
  self.passwordsInOtherAppsCoordinator = nil;
}

#pragma mark Private

- (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager {
  return IOSChromePasswordCheckManagerFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

@end

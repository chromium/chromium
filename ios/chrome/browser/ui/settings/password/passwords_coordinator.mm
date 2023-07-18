// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/password_checkup_metrics.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_settings_commands.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::WarningType;

@interface PasswordsCoordinator () <
    AddPasswordCoordinatorDelegate,
    PasswordDetailsCoordinatorDelegate,
    PasswordIssuesCoordinatorDelegate,
    PasswordCheckupCoordinatorDelegate,
    PasswordsInOtherAppsCoordinatorDelegate,
    PasswordSettingsCoordinatorDelegate,
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

// Coordinator for Password Checkup.
@property(nonatomic, strong)
    PasswordCheckupCoordinator* passwordCheckupCoordinator;

// Coordinator for password issues.
// TODO(crbug.com/1406871): Remove when kIOSPasswordCheckup is enabled by
// default.
@property(nonatomic, strong)
    PasswordIssuesCoordinator* passwordIssuesCoordinator;

// Coordinator for editing existing password details.
@property(nonatomic, strong)
    PasswordDetailsCoordinator* passwordDetailsCoordinator;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Coordinator for add password details.
@property(nonatomic, strong) AddPasswordCoordinator* addPasswordCoordinator;

// Coordinator for passwords in other apps promotion view.
@property(nonatomic, strong)
    PasswordsInOtherAppsCoordinator* passwordsInOtherAppsCoordinator;

@property(nonatomic, strong)
    PasswordSettingsCoordinator* passwordSettingsCoordinator;

// Modal alert for interactions with passwords list.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

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

  password_manager::LogStartPasswordCheckAutomatically();
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
      initWithPasswordCheckManager:IOSChromePasswordCheckManagerFactory::
                                       GetForBrowserState(browserState)
                  syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                       browserState)
                     faviconLoader:faviconLoader
                       syncService:SyncServiceFactory::GetForBrowserState(
                                       browserState)];
  self.reauthModule = [[ReauthenticationModule alloc]
      initWithSuccessfulReauthTimeAccessor:self.mediator];
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  self.passwordsViewController = [[PasswordManagerViewController alloc]
      initWithChromeAccountManagerService:accountManagerService
                              prefService:browserState->GetPrefs()];

  self.passwordsViewController.handler = self;
  self.passwordsViewController.delegate = self.mediator;
  self.passwordsViewController.dispatcher = self.dispatcher;
  self.passwordsViewController.presentationDelegate = self;
  self.passwordsViewController.reauthenticationModule = self.reauthModule;
  self.passwordsViewController.imageDataSource = self.mediator;

  self.mediator.consumer = self.passwordsViewController;

  [self.baseNavigationController pushViewController:self.passwordsViewController
                                           animated:YES];

  // When kIOSPasswordCheckup is enabled, start a password check.
  if (password_manager::features::IsPasswordCheckupEnabled()) {
    [self checkSavedPasswords];
  }
}

- (void)stop {
  self.passwordsViewController.delegate = nil;
  self.passwordsViewController = nil;

  [self.passwordCheckupCoordinator stop];
  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;

  [self.passwordIssuesCoordinator stop];
  self.passwordIssuesCoordinator.delegate = nil;
  self.passwordIssuesCoordinator = nil;

  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;

  [self.passwordsInOtherAppsCoordinator stop];
  self.passwordsInOtherAppsCoordinator.delegate = nil;
  self.passwordsInOtherAppsCoordinator = nil;

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.mediator disconnect];
}

#pragma mark - PasswordsSettingsCommands

- (void)showPasswordCheckup {
  DCHECK(!self.passwordCheckupCoordinator);
  self.passwordCheckupCoordinator = [[PasswordCheckupCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                          reauthModule:self.reauthModule
                              referrer:password_manager::PasswordCheckReferrer::
                                           kPasswordSettings];
  self.passwordCheckupCoordinator.delegate = self;
  [self.passwordCheckupCoordinator start];
}

// TODO(crbug.com/1464966): Make sure there aren't mutiple active
// `passwordIssuesCoordinator`s at once.
- (void)showPasswordIssues {
  self.passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:WarningType::kCompromisedPasswordsWarning
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  self.passwordIssuesCoordinator.delegate = self;
  self.passwordIssuesCoordinator.reauthModule = self.reauthModule;
  [self.passwordIssuesCoordinator start];
}

- (void)showDetailedViewForCredential:
    (const password_manager::CredentialUIEntry&)credential {
  DCHECK(!self.passwordDetailsCoordinator);
  self.passwordDetailsCoordinator = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                            credential:credential
                          reauthModule:self.reauthModule
                               context:DetailsContext::kPasswordSettings];
  self.passwordDetailsCoordinator.delegate = self;
  [self.passwordDetailsCoordinator start];
}

- (void)showDetailedViewForAffiliatedGroup:
    (const password_manager::AffiliatedGroup&)affiliatedGroup {
  DCHECK(!self.passwordDetailsCoordinator);
  self.passwordDetailsCoordinator = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                       affiliatedGroup:affiliatedGroup
                          reauthModule:self.reauthModule
                               context:DetailsContext::kPasswordSettings];
  self.passwordDetailsCoordinator.delegate = self;
  [self.passwordDetailsCoordinator start];
}

- (void)showAddPasswordSheet {
  DCHECK(!self.addPasswordCoordinator);
  self.addPasswordCoordinator = [[AddPasswordCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                    reauthModule:self.reauthModule];
  self.addPasswordCoordinator.delegate = self;
  [self.addPasswordCoordinator start];
}

- (void)showPasswordDeleteDialogWithOrigins:(NSArray<NSString*>*)origins
                                 completion:(void (^)(void))completion {
  std::pair<NSString*, NSString*> titleAndMessage =
      GetPasswordAlertTitleAndMessageForOrigins(origins);
  NSString* title = titleAndMessage.first;
  NSString* message = titleAndMessage.second;

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:title
                         message:message
                   barButtonItem:self.passwordsViewController.deleteButton];

  NSString* deleteButtonString =
      l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE);

  [self.actionSheetCoordinator addItemWithTitle:deleteButtonString
                                         action:^{
                                           completion();
                                         }
                                          style:UIAlertActionStyleDestructive];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_DELETION)
                action:nil
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

- (void)showSetupPasscodeDialog {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT);
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];

  __weak __typeof(self) weakSelf = self;
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kPasscodeArticleURL)];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                action:^{
                  [weakSelf.dispatcher closeSettingsUIAndOpenURL:command];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

#pragma mark - PasswordManagerViewControllerPresentationDelegate

- (void)PasswordManagerViewControllerDismissed {
  [self.delegate passwordsCoordinatorDidRemove:self];
}

- (void)showPasswordSettingsSubmenu {
  DCHECK(!self.passwordSettingsCoordinator);
  self.passwordSettingsCoordinator = [[PasswordSettingsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.passwordSettingsCoordinator.delegate = self;

  base::RecordAction(base::UserMetricsAction("PasswordManager_OpenSettings"));
  [self.passwordSettingsCoordinator start];
}

// TODO(crbug.com/1406871): Remove when kIOSPasswordCheckup is enabled by
// default.
#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  DCHECK_EQ(self.passwordIssuesCoordinator, coordinator);
  [self.passwordIssuesCoordinator stop];
  self.passwordIssuesCoordinator.delegate = nil;
  self.passwordIssuesCoordinator = nil;
}

#pragma mark - PasswordCheckupCoordinatorDelegate

- (void)passwordCheckupCoordinatorDidRemove:
    (PasswordCheckupCoordinator*)coordinator {
  DCHECK_EQ(self.passwordCheckupCoordinator, coordinator);
  [self.passwordCheckupCoordinator stop];
  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;
}

#pragma mark PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetailsCoordinator, coordinator);
  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;
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
    (const password_manager::CredentialUIEntry&)credential {
  [self.passwordsViewController
      setMostRecentlyUpdatedPasswordDetails:credential];
}

- (void)dismissAddViewControllerAndShowPasswordDetails:
            (const password_manager::CredentialUIEntry&)credential
                                           coordinator:(AddPasswordCoordinator*)
                                                           coordinator {
  DCHECK(self.addPasswordCoordinator &&
         self.addPasswordCoordinator == coordinator);
  [self passwordDetailsTableViewControllerDidFinish:coordinator];
  [self showDetailedViewForCredential:credential];
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

#pragma mark - PasswordSettingsCoordinatorDelegate

- (void)passwordSettingsCoordinatorDidRemove:
    (PasswordSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordSettingsCoordinator, coordinator);
  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;
}

@end

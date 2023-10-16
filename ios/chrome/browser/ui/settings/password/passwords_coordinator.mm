// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_settings_commands.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_coordinator.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::WarningType;

@interface PasswordsCoordinator () <
    AddPasswordCoordinatorDelegate,
    PasswordDetailsCoordinatorDelegate,
    PasswordIssuesCoordinatorDelegate,
    PasswordCheckupCoordinatorDelegate,
    PasswordSettingsCoordinatorDelegate,
    PasswordsSettingsCommands,
    PasswordManagerViewControllerPresentationDelegate,
    ReauthenticationCoordinatorDelegate,
    WidgetPromoInstructionsCoordinatorDelegate>

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

@property(nonatomic, strong)
    PasswordSettingsCoordinator* passwordSettingsCoordinator;

// Coordinator for blocking password manager until successful Local
// Authentication.
@property(nonatomic, strong) ReauthenticationCoordinator* reauthCoordinator;

// Modal alert for interactions with passwords list.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Coordinator that presents the instructions on how to install the Password
// Manager widget.
@property(nonatomic, strong)
    WidgetPromoInstructionsCoordinator* widgetPromoInstructionsCoordinator;

// Indicates that a password manager visit metric has been recorded.
// Used to only record the metric the first time authentication is passed.
@property(nonatomic) BOOL recordedPasswordManagerVisit;

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
    _recordedPasswordManagerVisit = NO;
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
                     faviconLoader:faviconLoader
                       syncService:SyncServiceFactory::GetForBrowserState(
                                       browserState)
                       prefService:browserState->GetPrefs()];
  self.mediator.tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);

  self.reauthModule = password_manager::BuildReauthenticationModule(
      /*successfulReauthTimeAccessor=*/self.mediator);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);

  PasswordManagerViewController* passwordsViewController =
      [[PasswordManagerViewController alloc]
          initWithChromeAccountManagerService:accountManagerService
                                  prefService:browserState->GetPrefs()
                       shouldOpenInSearchMode:
                           self.openViewControllerForPasswordSearch];
  self.passwordsViewController = passwordsViewController;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  passwordsViewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  passwordsViewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  passwordsViewController.browsingDataHandler =
      HandlerForProtocol(dispatcher, BrowsingDataCommands);
  passwordsViewController.settingsHandler =
      HandlerForProtocol(dispatcher, ApplicationSettingsCommands);
  passwordsViewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  passwordsViewController.handler = self;
  passwordsViewController.delegate = self.mediator;
  passwordsViewController.presentationDelegate = self;
  passwordsViewController.reauthenticationModule = self.reauthModule;
  passwordsViewController.imageDataSource = self.mediator;

  self.mediator.consumer = self.passwordsViewController;

  BOOL startBlockedForReauth =
      password_manager::features::IsAuthOnEntryEnabled() ||
      password_manager::features::IsAuthOnEntryV2Enabled();
  // Disable animation when content will be blocked for reauth to prevent
  // flickering in navigation bar.
  [self.baseNavigationController pushViewController:self.passwordsViewController
                                           animated:!startBlockedForReauth];

  if (startBlockedForReauth) {
    [self startReauthCoordinatorWithAuthOnStart:YES];
  } else {
    [self recordPasswordManagerVisitIfNeeded];
  }

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

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.addPasswordCoordinator stop];
  self.addPasswordCoordinator.delegate = nil;
  self.addPasswordCoordinator = nil;

  [self.reauthCoordinator stop];
  self.reauthCoordinator.delegate = nil;
  self.reauthCoordinator = nil;
  [self dismissActionSheetCoordinator];
  [self dismissAlertCoordinator];

  [self.mediator disconnect];
}

#pragma mark - PasswordsSettingsCommands

- (void)showPasswordCheckup {
  DCHECK(!self.passwordCheckupCoordinator);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  self.passwordCheckupCoordinator = [[PasswordCheckupCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                          reauthModule:self.reauthModule
                              referrer:password_manager::PasswordCheckReferrer::
                                           kPasswordSettings];
  self.passwordCheckupCoordinator.delegate = self;
  [self.passwordCheckupCoordinator start];
}

- (void)showPasswordIssues {
  // TODO(crbug.com/1464966): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.passwordIssuesCoordinator);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  self.passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:WarningType::kCompromisedPasswordsWarning
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  _passwordIssuesCoordinator.skipAuthenticationOnStart = YES;
  _passwordIssuesCoordinator.delegate = self;
  _passwordIssuesCoordinator.reauthModule = self.reauthModule;
  [_passwordIssuesCoordinator start];
}

- (void)showDetailedViewForCredential:
    (const password_manager::CredentialUIEntry&)credential {
  DCHECK(!self.passwordDetailsCoordinator);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

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
  // TODO(crbug.com/1464966): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.passwordDetailsCoordinator);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];
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
  // TODO(crbug.com/1464966): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.addPasswordCoordinator);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];
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
      password_manager::GetPasswordAlertTitleAndMessageForOrigins(origins);
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
  __weak PasswordsCoordinator* weakSelf = self;

  [self.actionSheetCoordinator addItemWithTitle:deleteButtonString
                                         action:^{
                                           completion();
                                           [weakSelf
                                               dismissActionSheetCoordinator];
                                         }
                                          style:UIAlertActionStyleDestructive];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_DELETION)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
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
                                   action:^{
                                     [weakSelf dismissAlertCoordinator];
                                   }
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                action:^{
                  [weakSelf.dispatcher closeSettingsUIAndOpenURL:command];
                  [weakSelf dismissAlertCoordinator];
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

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  self.passwordSettingsCoordinator = [[PasswordSettingsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.passwordSettingsCoordinator.delegate = self;
  // No auth required as Passwords Coordinator already is auth protected.
  self.passwordSettingsCoordinator.skipAuthenticationOnStart = YES;

  base::RecordAction(base::UserMetricsAction("PasswordManager_OpenSettings"));
  [self.passwordSettingsCoordinator start];
}

- (void)showPasswordManagerWidgetPromoInstructions {
  // TODO(crbug.com/1464966): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.widgetPromoInstructionsCoordinator);

  // TODO(crbug.com/1486873): Validate that reauth coordinator should be stopped
  // here.
  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  self.widgetPromoInstructionsCoordinator =
      [[WidgetPromoInstructionsCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  self.widgetPromoInstructionsCoordinator.delegate = self;
  [self.widgetPromoInstructionsCoordinator start];
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
  [self restartReauthCoordinator];
}

#pragma mark - PasswordCheckupCoordinatorDelegate

- (void)passwordCheckupCoordinatorDidRemove:
    (PasswordCheckupCoordinator*)coordinator {
  DCHECK_EQ(self.passwordCheckupCoordinator, coordinator);
  [self.passwordCheckupCoordinator stop];
  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;
  [self restartReauthCoordinator];
}

#pragma mark PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetailsCoordinator, coordinator);
  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;
  [self restartReauthCoordinator];
}

#pragma mark AddPasswordDetailsCoordinatorDelegate

- (void)passwordDetailsTableViewControllerDidFinish:
    (AddPasswordCoordinator*)coordinator {
  DCHECK_EQ(self.addPasswordCoordinator, coordinator);
  [self.addPasswordCoordinator stop];
  self.addPasswordCoordinator.delegate = nil;
  self.addPasswordCoordinator = nil;
  [self restartReauthCoordinator];
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

#pragma mark - PasswordSettingsCoordinatorDelegate

- (void)passwordSettingsCoordinatorDidRemove:
    (PasswordSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordSettingsCoordinator, coordinator);
  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self restartReauthCoordinator];
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  DCHECK_EQ(_reauthCoordinator, coordinator);

  [self recordPasswordManagerVisitIfNeeded];

  [self.mediator askFETToShowPasswordManagerWidgetPromo];

  // Cleanup reauthCoordinator if scene state monitoring is not enabled.
  if (!password_manager::features::IsAuthOnEntryV2Enabled()) {
    [_reauthCoordinator stop];
    _reauthCoordinator.delegate = nil;
    _reauthCoordinator = nil;
  }
}

- (void)willPushReauthenticationViewController {
  [self dismissAlertCoordinator];
  [self dismissActionSheetCoordinator];
}

#pragma mark - WidgetPromoInstructionsCoordinatorDelegate

- (void)removeWidgetPromoInstructionsCoordinator:
    (WidgetPromoInstructionsCoordinator*)coordinator {
  DCHECK_EQ(self.widgetPromoInstructionsCoordinator, coordinator);
  [self.widgetPromoInstructionsCoordinator stop];
  self.widgetPromoInstructionsCoordinator.delegate = nil;
  self.widgetPromoInstructionsCoordinator = nil;
  [self restartReauthCoordinator];
}

#pragma mark - Private

// Starts reauthCoordinator.
// - authOnStart: Pass `YES` to cover password manager with an empty view
// controller until successful Local Authentication when reauthCoordinator
// starts.
//
// Local authentication is required everytime the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinatorWithAuthOnStart:(BOOL)authOnStart {
  // At this point we are either starting the PasswordsCoordinator or we have
  // just dismissed a child coordinator. If the previous reauth coordinator was
  // not stopped and deallocated when the child coordinator was started, we
  // would have multiple reauth coordinators listening for scene states and
  // triggering reauth at the same time with undefined behavior.
  DCHECK(!_reauthCoordinator);

  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser
                reauthenticationModule:_reauthModule
                           authOnStart:authOnStart];

  _reauthCoordinator.delegate = self;

  [_reauthCoordinator start];
}

// Stop reauth coordinator when a child coordinator will be started.
//
// Needed so reauth coordinator doesn't block for reauth if the scene state
// changes while the child coordinator is presenting its content. The child
// coordinator will add its own reauth coordinator to block its content for
// reauth.
- (void)stopReauthCoordinatorBeforeStartingChildCoordinator {
  // Popping the view controller in case Local Authentication was triggered
  // outside reauthCoordinator before starting the child coordinator. Local
  // Authentication changes the scene state which triggers the presentation of
  // the ReauthenticationViewController by reauthCoordinator. Ideally
  // reauthCoordinator would be stopped when Local Authentication is triggered
  // outside of it but still defending against that scenario to avoid leaving an
  // unintended view controller in the navigation stack.
  [_reauthCoordinator stopAndPopViewController];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

// Starts reauthCoordinator after a child coordinator content was dismissed.
- (void)restartReauthCoordinator {
  // Restart reauth coordinator so it monitors scene state changes and requests
  // local authentication after the scene goes to the background.
  if (password_manager::features::IsAuthOnEntryV2Enabled()) {
    [self startReauthCoordinatorWithAuthOnStart:NO];
  }
}

// Records password manager visit metric.
// Only records the first time it is called during the lifetime of self, no-op
// after that.
- (void)recordPasswordManagerVisitIfNeeded {
  if (_recordedPasswordManagerVisit) {
    return;
  }
  // Record only once during the lifetime of self.
  _recordedPasswordManagerVisit = YES;
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.iOS.PasswordManagerVisit", true);
}

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

- (void)dismissAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

@end

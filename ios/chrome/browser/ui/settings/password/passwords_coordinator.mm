// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"

#import "base/debug/dump_without_crashing.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
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

// Coordinator for Password Checkup.
@property(nonatomic, strong)
    PasswordCheckupCoordinator* passwordCheckupCoordinator;

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

// Coordinator that presents the instructions on how to install the Password
// Manager widget.
@property(nonatomic, strong)
    WidgetPromoInstructionsCoordinator* widgetPromoInstructionsCoordinator;

@end

@implementation PasswordsCoordinator {
  // For recording visits after successful authentication.
  IOSPasswordManagerVisitsRecorder* _visitsRecorder;

  // Whether local authentication failed for a child coordinator and thus the
  // whole Password Manager UI is being dismissed.
  BOOL _authDidFailForChildCoordinator;
}

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

- (void)checkSavedPasswords {
  [self.mediator startPasswordCheck];

  password_manager::LogStartPasswordCheckAutomatically();
}

- (UIViewController*)viewController {
  return self.passwordsViewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);

  self.mediator = [[PasswordsMediator alloc]
      initWithPasswordCheckManager:IOSChromePasswordCheckManagerFactory::
                                       GetForProfile(profile)
                     faviconLoader:faviconLoader
                       syncService:SyncServiceFactory::GetForProfile(profile)
                       prefService:profile->GetPrefs()];
  self.mediator.tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);

  self.reauthModule = password_manager::BuildReauthenticationModule(
      /*successfulReauthTimeAccessor=*/self.mediator);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  PasswordManagerViewController* passwordsViewController =
      [[PasswordManagerViewController alloc]
          initWithChromeAccountManagerService:accountManagerService
                                  prefService:profile->GetPrefs()
                       shouldOpenInSearchMode:
                           self.openViewControllerForPasswordSearch];
  self.passwordsViewController = passwordsViewController;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  passwordsViewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  passwordsViewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  passwordsViewController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  passwordsViewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  passwordsViewController.handler = self;
  passwordsViewController.delegate = self.mediator;
  passwordsViewController.presentationDelegate = self;
  passwordsViewController.reauthenticationModule = self.reauthModule;
  passwordsViewController.imageDataSource = self.mediator;

  self.mediator.consumer = self.passwordsViewController;

  // Disable animation when content will be blocked for reauth to prevent
  // flickering in navigation bar.
  [self.baseNavigationController pushViewController:self.passwordsViewController
                                           animated:NO];

  _visitsRecorder = [[IOSPasswordManagerVisitsRecorder alloc]
      initWithPasswordManagerSurface:password_manager::PasswordManagerSurface::
                                         kPasswordList];

  [self startReauthCoordinatorWithAuthOnStart:YES];

  // Start a password check.
  [self checkSavedPasswords];
}

- (void)stop {
  self.passwordsViewController.delegate = nil;
  self.passwordsViewController = nil;

  [self.passwordCheckupCoordinator stop];
  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;

  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;

  // When the coordinator is stopped due to failed authentication, the whole
  // Password Manager UI is dismissed via command. Not dismissing the top
  // presented coordinator UI before everything else prevents the Password
  // Manager UI from being visible without local authentication.
  [self.passwordSettingsCoordinator
      stopWithUIDismissal:!_authDidFailForChildCoordinator];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.addPasswordCoordinator
      stopWithUIDismissal:!_authDidFailForChildCoordinator];
  self.addPasswordCoordinator.delegate = nil;
  self.addPasswordCoordinator = nil;

  [self.reauthCoordinator stop];
  self.reauthCoordinator.delegate = nil;
  self.reauthCoordinator = nil;
  [self dismissActionSheetCoordinator];

  [self.mediator disconnect];
}

#pragma mark - PasswordsSettingsCommands

- (void)showPasswordCheckup {
  if (self.passwordCheckupCoordinator &&
      self.baseNavigationController.topViewController !=
          self.passwordsViewController) {
    base::debug::DumpWithoutCrashing();
  }

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

- (void)showDetailedViewForCredential:
    (const password_manager::CredentialUIEntry&)credential {
  if (self.passwordDetailsCoordinator &&
      self.baseNavigationController.topViewController !=
          self.passwordsViewController) {
    base::debug::DumpWithoutCrashing();
  }

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
  if (self.passwordDetailsCoordinator &&
      self.baseNavigationController.topViewController !=
          self.passwordsViewController) {
    base::debug::DumpWithoutCrashing();
  }

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
  if (self.addPasswordCoordinator &&
      self.baseNavigationController.topViewController !=
          self.passwordsViewController) {
    base::debug::DumpWithoutCrashing();
  }

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];
  self.addPasswordCoordinator = [[AddPasswordCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
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

#pragma mark - PasswordManagerViewControllerPresentationDelegate

- (void)PasswordManagerViewControllerDismissed {
  [self.delegate passwordsCoordinatorDidRemove:self];
}

- (void)showPasswordSettingsSubmenu {
  if (self.passwordSettingsCoordinator &&
      self.baseNavigationController.topViewController !=
          self.passwordsViewController) {
    base::debug::DumpWithoutCrashing();
  }

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
  if (self.widgetPromoInstructionsCoordinator &&
      self.baseNavigationController.topViewController !=
          self.passwordsViewController) {
    base::debug::DumpWithoutCrashing();
  }

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  self.widgetPromoInstructionsCoordinator =
      [[WidgetPromoInstructionsCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  self.widgetPromoInstructionsCoordinator.delegate = self;
  [self.widgetPromoInstructionsCoordinator start];
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

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  _authDidFailForChildCoordinator = YES;
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
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

  [_visitsRecorder maybeRecordVisitMetric];

  [self.mediator askFETToShowPasswordManagerWidgetPromo];

  // Make sure that the Password Manager's toolbar is in the correct state once
  // the reauthentication view controller is dismissed. This is a fix for
  // crbug.com/1503081 that works well in pratice, but isn't perfect due to
  // possible race conditions.
  if (_baseNavigationController.topViewController ==
      self.passwordsViewController) {
    [self.passwordsViewController updateUIForEditState];
  }
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);

  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)willPushReauthenticationViewController {
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
  [self startReauthCoordinatorWithAuthOnStart:NO];
}

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_coordinator.h"

#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_mediator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_issues/password_issues_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/reauthentication/local_reauthentication_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using password_manager::PasswordCheckReferrer;

@interface PasswordCheckupCoordinator () <
    PasswordCheckupCommands,
    PasswordCheckupMediatorDelegate,
    PasswordIssuesCoordinatorDelegate,
    NotificationsSettingsObserverDelegate,
    LocalReauthenticationCoordinatorDelegate,
    UINavigationControllerDelegate>

@end

@implementation PasswordCheckupCoordinator {
  // Main view controller for this coordinator.
  PasswordCheckupViewController* _viewController;

  // Main mediator for this coordinator.
  PasswordCheckupMediator* _mediator;

  // Coordinator for password issues.
  PasswordIssuesCoordinator* _passwordIssuesCoordinator;

  // Reauthentication module used by password issues coordinator.
  id<ReauthenticationProtocol> _reauthModule;

  // Coordinator for blocking Password Checkup until Local Authentication is
  // passed. Used for requiring authentication when opening Password Checkup
  // from outside the Password Manager and when the app is
  // backgrounded/foregrounded with Password Checkup opened.
  LocalReauthenticationCoordinator* _reauthCoordinator;

  // Location in the app from which Password Checkup was opened.
  PasswordCheckReferrer _referrer;

  // For recording visits after successful authentication.
  IOSPasswordManagerVisitsRecorder* _visitsRecorder;

  // An observer that tracks whether push notification permission settings have
  // been modified.
  NotificationsSettingsObserver* _notificationsSettingsObserver;

  // The service responsible for running the password checks.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                        reauthModule:(id<ReauthenticationProtocol>)reauthModule
                            referrer:(PasswordCheckReferrer)referrer {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    // The Password Checkup homepage is not intended to be visited by signed out
    // users. However, if it does happen, it's preferable to show the signed-out
    // state UI rather than crash the app.
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(self.profile);
    CHECK(authenticationService);
    if (!authenticationService->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin)) {
      base::debug::DumpWithoutCrashing();
    }

    _baseNavigationController = navigationController;
    _reauthModule = reauthModule;
    _dispatcher =
        HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
    _referrer = referrer;
    password_manager::LogPasswordCheckReferrer(referrer);

    _notificationsSettingsObserver = [[NotificationsSettingsObserver alloc]
        initWithPrefService:self.profile->GetPrefs()
                 localState:GetApplicationContext()->GetLocalState()];
    _notificationsSettingsObserver.delegate = self;
  }
  return self;
}

- (void)start {
  [super start];

  password_manager::LogOpenPasswordCheckupHomePage();
  _viewController = [[PasswordCheckupViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.handler = self;

  _passwordCheckManager =
      IOSChromePasswordCheckManagerFactory::GetForProfile(self.profile);
  _mediator = [[PasswordCheckupMediator alloc]
      initWithPasswordCheckManager:_passwordCheckManager];
  _viewController.delegate = _mediator;
  _mediator.consumer = _viewController;
  _mediator.delegate = self;

  BOOL requireAuthOnStart = [self shouldRequireAuthOnStart];

  _baseNavigationController.delegate = self;

  // Disable animation when content will be blocked for reauth to prevent
  // flickering in navigation bar.
  [self.baseNavigationController pushViewController:_viewController
                                           animated:!requireAuthOnStart];

  _visitsRecorder = [[IOSPasswordManagerVisitsRecorder alloc]
      initWithPasswordManagerSurface:password_manager::PasswordManagerSurface::
                                         kPasswordCheckup];

  // Only record visit if no auth is required, otherwise wait for successful
  // auth.
  if (!requireAuthOnStart) {
    [_visitsRecorder maybeRecordVisitMetric];
  }

  [self startReauthCoordinatorWithAuthOnStart:requireAuthOnStart];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController.handler = nil;
  _viewController = nil;

  // Remove `PrefObserverDelegate`s.
  _notificationsSettingsObserver.delegate = nil;
  [_notificationsSettingsObserver disconnect];
  _notificationsSettingsObserver = nil;

  [self stopPasswordIssuesCoordinator];
  [self stopReauthenticationCoordinator];

  _baseNavigationController.delegate = nil;
}

#pragma mark - PasswordCheckupCommands

- (void)dismissPasswordCheckupViewController {
  [self.delegate passwordCheckupCoordinatorDidRemove:self];
}

// Opens the Password Issues list displaying compromised, weak or reused
// credentials for `warningType`.
- (void)showPasswordIssuesWithWarningType:
    (password_manager::WarningType)warningType {
  DUMP_WILL_BE_CHECK(!_passwordIssuesCoordinator);

  [self stopReauthCoordinatorBeforeStartingChildCoordinator];

  password_manager::LogOpenPasswordIssuesList(warningType);

  // Prevent actions temporarily until the password issues VC takes over the
  // stack.
  [_viewController startCooldown];

  _passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
            initForWarningType:warningType
      baseNavigationController:self.baseNavigationController
                       browser:self.browser];
  // No need to authenticate the user before showing password issues as the user
  // was already authenticated when opening the password manager.
  _passwordIssuesCoordinator.skipAuthenticationOnStart = YES;
  _passwordIssuesCoordinator.delegate = self;
  _passwordIssuesCoordinator.reauthModule = _reauthModule;
  [_passwordIssuesCoordinator start];
}

- (void)dismissAndOpenURL:(CrURL*)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [self.dispatcher closePresentedViewsAndOpenURL:command];
}

- (void)dismissAfterAllPasswordsGone {
  NSArray<UIViewController*>* viewControllers =
      self.baseNavigationController.viewControllers;
  NSInteger viewControllerIndex =
      [viewControllers indexOfObject:_viewController];

  // Nothing to do if the view controller was already removed from the
  // navigation stack.
  if (viewControllerIndex == NSNotFound) {
    return;
  }

  // Dismiss the whole navigation stack if checkup is the root view controller.
  if (viewControllerIndex == 0) {
    UIViewController* presentingViewController =
        _baseNavigationController.presentingViewController;
    [presentingViewController dismissViewControllerAnimated:YES completion:nil];
    return;
  }

  // Go to the previous view controller in the navigation stack.
  [self.baseNavigationController
      popToViewController:viewControllers[viewControllerIndex - 1]
                 animated:YES];
}

#pragma mark - PasswordCheckupMediatorDelegate

- (void)toggleSafetyCheckNotifications {
  if ([self isSafetyCheckNotificationsEnabled]) {
    [self disableSafetyCheckNotifications];
    return;
  }

  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands)
      showNotificationsOptInFromAccessPoint:NotificationOptInAccessPoint::
                                                kSafetyCheck
                         baseViewController:_baseNavigationController];
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  CHECK_EQ(_passwordIssuesCoordinator, coordinator);
  [self stopPasswordIssuesCoordinator];
  [self restartReauthCoordinator];
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

#pragma mark - NotificationsSettingsObserverDelegate

- (void)notificationsSettingsDidChangeForClient:
    (PushNotificationClientId)clientID {
  if (clientID == PushNotificationClientId::kSafetyCheck) {
    [_mediator
        reconfigureNotificationsSection:[self
                                            isSafetyCheckNotificationsEnabled]];
  }
}

#pragma mark - LocalReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (LocalReauthenticationCoordinator*)coordinator {
  [_visitsRecorder maybeRecordVisitMetric];
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (LocalReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);

  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)willPushReauthenticationViewController {
  // No-op.
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kPasswordCheckupUIDoubleStartMitigation)) {
    return;
  }

  // Enable user interactions when the `_viewController` (password checkup VC)
  // is at the top of the nav stack OR disable user interactions when
  // `_viewController` isn't at the top (e.g. the password issues VC is stacked
  // over it). This should be reliable where a situation where the
  // `_viewController` is forever disabled is almost impossible as all the
  // possible states are handled and the notification should be emitted on each
  // nav.
  if (viewController == _viewController) {
    // Re-enable user interaction because at this point (1) the child
    // coordinator was stopped and (2) the password checkup `_viewController` is
    // now visible at the top of the nav stack.
    _viewController.view.userInteractionEnabled = YES;
    // Use the cooldown period just in case.
    [_viewController startCooldown];
  } else if ([navigationController.viewControllers
                 containsObject:_viewController]) {
    // Disable user interactions on `_viewController` since there is a view on
    // top of it.
    _viewController.view.userInteractionEnabled = NO;
  }
}

#pragma mark - Private

// Returns `YES` if the user has opted in to receive Safety Check notifications.
- (BOOL)isSafetyCheckNotificationsEnabled {
  // Safety Check notifications are controlled by app-wide notification
  // settings, not profile-specific ones. No Gaia ID is required below in
  // `GetMobileNotificationPermissionStatusForClient()`.
  return push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(
          PushNotificationClientId::kSafetyCheck, GaiaId());
}

// Opts the user out of Safety Check notifications and updates the push
// notification service preferences. Displays a confirmation snackbar with a
// link to notification settings.
- (void)disableSafetyCheckNotifications {
  GetApplicationContext()->GetPushNotificationService()->SetPreference(
      GaiaId(), PushNotificationClientId::kSafetyCheck, false);

  // Show confirmation snackbar.
  NSString* buttonText =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_MANAGE_SETTINGS);

  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_NOTIFICATIONS_CONFIRMATION_MESSAGE_OFF,
      l10n_util::GetStringUTF16(IDS_IOS_SAFETY_CHECK_TITLE));

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  __weak id<SettingsCommands> weakSettingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);

  [snackbarHandler showSnackbarWithMessage:message
                                buttonText:buttonText
                             messageAction:^{
                               [weakSettingsHandler showNotificationsSettings];
                             }
                          completionAction:nil];
}

- (void)stopPasswordIssuesCoordinator {
  [_passwordIssuesCoordinator stop];
  _passwordIssuesCoordinator.delegate = nil;
  _passwordIssuesCoordinator = nil;
}

- (void)stopReauthenticationCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

// Starts reauthCoordinator.
// - authOnStart: Pass `YES` to cover Password Checkup with an empty view
// controller until successful Local Authentication when reauthCoordinator
// starts.
//
// Local authentication is required every time the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinatorWithAuthOnStart:(BOOL)authOnStart {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordCheckupCoordinatorShowReauth"));

  if (authOnStart && base::FeatureList::IsEnabled(
                         password_manager::features::
                             kPasswordCheckupUIDoubleStartMitigation)) {
    // Prevent actions temporarily in the case the auth view has to be
    // pushed on the stack. You don't want actions when auth is required.
    [_viewController startCooldown];
  }

  DCHECK(!_reauthCoordinator);

  _reauthCoordinator = [[LocalReauthenticationCoordinator alloc]
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
  // the LocalReauthenticationViewController by reauthCoordinator. Ideally
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

// Whether Local Authentication sould be required when the coordinator is
// started.
- (BOOL)shouldRequireAuthOnStart {
  // Request auth when opened from outside Password Manager.
  switch (_referrer) {
    case PasswordCheckReferrer::kSafetyCheck:
    case PasswordCheckReferrer::kPhishGuardDialog:
    case PasswordCheckReferrer::kPasswordBreachDialog:
    case PasswordCheckReferrer::kMoreToFixBubble:
    case PasswordCheckReferrer::kSafetyCheckMagicStack:
    case PasswordCheckReferrer::kSafetyCheckNotification:
      return YES;
    case PasswordCheckReferrer::kPasswordSettings:
      return NO;
  }
}

@end

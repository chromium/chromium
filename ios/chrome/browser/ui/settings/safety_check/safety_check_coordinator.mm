// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_ui_swift.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using password_manager::WarningType;

@interface SafetyCheckCoordinator () <
    PasswordCheckupCoordinatorDelegate,
    PopoverLabelViewControllerDelegate,
    PrivacySafeBrowsingCoordinatorDelegate,
    NotificationsOptInAlertCoordinatorDelegate,
    SafetyCheckNavigationCommands,
    SafetyCheckMediatorDelegate,
    SafetyCheckTableViewControllerPresentationDelegate>

// Safety check mediator.
@property(nonatomic, strong) SafetyCheckMediator* mediator;

// The container view controller.
@property(nonatomic, strong) SafetyCheckTableViewController* viewController;

// Coordinator for Password Checkup.
@property(nonatomic, strong)
    PasswordCheckupCoordinator* passwordCheckupCoordinator;

// Dispatcher which can handle changing passwords on sites.
@property(nonatomic, strong) id<ApplicationCommands> handler;

// Coordinator for the Privacy and Security screen (SafeBrowsing toggle
// location).
@property(nonatomic, strong)
    PrivacySafeBrowsingCoordinator* privacySafeBrowsingCoordinator;

// Where in the app the Safety Check was requested from.
@property(nonatomic, assign) password_manager::PasswordCheckReferrer referrer;

// Popover view controller with error information.
@property(nonatomic, strong)
    PopoverLabelViewController* errorInfoPopoverViewController;

@end

@implementation SafetyCheckCoordinator {
  // Alert Coordinator used to display the notifications system prompt.
  NotificationsOptInAlertCoordinator* _optInAlertCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            referrer:(password_manager::PasswordCheckReferrer)
                                         referrer {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                  ApplicationCommands);
    _referrer = referrer;
  }
  return self;
}

- (void)startCheckIfNotRunning {
  [self.mediator startCheckIfNotRunning];
}

#pragma mark - ChromeCoordinator

- (void)start {
  SafetyCheckTableViewController* viewController =
      [[SafetyCheckTableViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  self.viewController = viewController;

  scoped_refptr<IOSChromePasswordCheckManager> passwordCheckManager =
      IOSChromePasswordCheckManagerFactory::GetForProfile(
          self.browser->GetProfile());
  self.mediator = [[SafetyCheckMediator alloc]
      initWithUserPrefService:self.browser->GetProfile()->GetPrefs()
             localPrefService:GetApplicationContext()->GetLocalState()
         passwordCheckManager:passwordCheckManager
                  authService:AuthenticationServiceFactory::GetForProfile(
                                  self.browser->GetProfile())
                  syncService:SyncServiceFactory::GetForProfile(
                                  self.browser->GetProfile())
                     referrer:_referrer];

  self.mediator.consumer = self.viewController;
  self.mediator.handler = self;
  self.mediator.delegate = self;
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;

  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  // If the Safe Browsing Settings page was accessed through the Safe
  // Browsing row of the safety check, we need to explicity stop the
  // privacySafeBrowsingCoordinator before closing the settings window.
  [self.privacySafeBrowsingCoordinator stop];
  self.privacySafeBrowsingCoordinator.delegate = nil;
  self.privacySafeBrowsingCoordinator = nil;

  [self.passwordCheckupCoordinator stop];
  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;

  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = nil;
}

- (void)updateNotificationsButton:(BOOL)enabled {
  CHECK(IsSafetyCheckNotificationsEnabled());

  [self.mediator reconfigureNotificationsSection:enabled];
}

#pragma mark - SafetyCheckMediatorDelegate

- (void)toggleSafetyCheckNotifications {
  CHECK(IsSafetyCheckNotificationsEnabled());

  // Safety Check notifications are controlled by app-wide notification
  // settings, not profile-specific ones. No Gaia ID is required below in
  // `GetMobileNotificationPermissionStatusForClient()`.
  if (push_notification_settings::
          GetMobileNotificationPermissionStatusForClient(
              PushNotificationClientId::kSafetyCheck, "")) {
    [self disableNotifications];

    return;
  }

  [self enableNotifications];
}

#pragma mark - NotificationsOptInAlertCoordinatorDelegate

- (void)notificationsOptInAlertCoordinator:
            (NotificationsOptInAlertCoordinator*)alertCoordinator
                                    result:
                                        (NotificationsOptInAlertResult)result {
  CHECK_EQ(_optInAlertCoordinator, alertCoordinator);
  [_optInAlertCoordinator stop];
  _optInAlertCoordinator = nil;

  switch (result) {
    case NotificationsOptInAlertResult::kPermissionGranted:
      [_mediator reconfigureNotificationsSection:YES];
      break;
    case NotificationsOptInAlertResult::kPermissionDenied:
    case NotificationsOptInAlertResult::kOpenedSettings:
    case NotificationsOptInAlertResult::kCanceled:
    case NotificationsOptInAlertResult::kError:
      [_mediator reconfigureNotificationsSection:NO];
      break;
  }
}

#pragma mark - SafetyCheckTableViewControllerPresentationDelegate

- (void)safetyCheckTableViewControllerDidRemove:
    (SafetyCheckTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate safetyCheckCoordinatorDidRemove:self];
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  GURL convertedURL = net::GURLWithNSURL(URL);
  const GURL safeBrowsingURL(
      base::SysNSStringToUTF8(kSafeBrowsingSafetyCheckStringURL));

  // Take the user to Sync and Google Services page in Bling instead of desktop
  // settings.
  if (convertedURL == safeBrowsingURL) {
    [self.errorInfoPopoverViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [self showSafeBrowsingPreferencePage];
                           }];
    return;
  }

  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:convertedURL];
  [self.handler closeSettingsUIAndOpenURL:command];
}

#pragma mark - SafetyCheckNavigationCommands

- (void)showPasswordCheckupPage {
  DUMP_WILL_BE_CHECK(!self.passwordCheckupCoordinator);
  self.passwordCheckupCoordinator = [[PasswordCheckupCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                          reauthModule:nil
                              referrer:password_manager::PasswordCheckReferrer::
                                           kSafetyCheck];
  self.passwordCheckupCoordinator.delegate = self;
  [self.passwordCheckupCoordinator start];
}

- (void)showErrorInfoFrom:(UIButton*)buttonView
                 withText:(NSAttributedString*)text {
  self.errorInfoPopoverViewController =
      [[PopoverLabelViewController alloc] initWithPrimaryAttributedString:text
                                                secondaryAttributedString:nil];

  self.errorInfoPopoverViewController.delegate = self;

  self.errorInfoPopoverViewController.popoverPresentationController.sourceView =
      buttonView;
  self.errorInfoPopoverViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  self.errorInfoPopoverViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionAny;
  [self.viewController presentViewController:self.errorInfoPopoverViewController
                                    animated:YES
                                  completion:nil];
}

- (void)showUpdateAtLocation:(NSString*)location {
  if (!location) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  const GURL url(base::SysNSStringToUTF8(location));
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  [self.handler closeSettingsUIAndOpenURL:command];
}

- (void)showSafeBrowsingPreferencePage {
  DCHECK(!self.privacySafeBrowsingCoordinator);
  base::RecordAction(
      base::UserMetricsAction("Settings.SafetyCheck.ManageSafeBrowsing"));
  base::UmaHistogramEnumeration("Settings.SafetyCheck.Interactions",
                                SafetyCheckInteractions::kSafeBrowsingManage);
  self.privacySafeBrowsingCoordinator = [[PrivacySafeBrowsingCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.privacySafeBrowsingCoordinator.delegate = self;
  [self.privacySafeBrowsingCoordinator start];
}

- (void)showManagedInfoFrom:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  [self.viewController presentViewController:bubbleViewController
                                    animated:YES
                                  completion:nil];

  // Disable the button when showing the bubble.
  // The button will be enabled when close the bubble in
  // (void)popoverPresentationControllerDidDismissPopover: of
  // EnterpriseInfoPopoverViewController.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
}

#pragma mark - PasswordCheckupCoordinatorDelegate

- (void)passwordCheckupCoordinatorDidRemove:
    (PasswordCheckupCoordinator*)coordinator {
  DCHECK_EQ(self.passwordCheckupCoordinator, coordinator);
  [self.passwordCheckupCoordinator stop];
  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  // Pop everything up to the Safety Check page.
  // When there is content presented, don't animate the dismissal of the view
  // controllers in the navigation controller to prevent revealing passwords
  // when the presented content is the one covered by the reauthentication UI.
  UINavigationController* navigationController = self.baseNavigationController;
  UIViewController* topViewController = navigationController.topViewController;
  UIViewController* presentedViewController =
      topViewController.presentedViewController;

  [navigationController popToViewController:_viewController
                                   animated:presentedViewController == nil];

  [presentedViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

#pragma mark - PrivacySafeBrowsingCoordinatorDelegate

- (void)privacySafeBrowsingCoordinatorDidRemove:
    (PrivacySafeBrowsingCoordinator*)coordinator {
  DCHECK_EQ(_privacySafeBrowsingCoordinator, coordinator);
  [self.privacySafeBrowsingCoordinator stop];
  self.privacySafeBrowsingCoordinator.delegate = nil;
  self.privacySafeBrowsingCoordinator = nil;
}

#pragma mark - Private methods

// Prompts the user to opt-in to Safety Check push notifications.
// If the user grants permission, updates the push notification service
// preferences.
- (void)enableNotifications {
  CHECK(IsSafetyCheckNotificationsEnabled());

  [_optInAlertCoordinator stop];

  _optInAlertCoordinator = [[NotificationsOptInAlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];

  _optInAlertCoordinator.delegate = self;

  _optInAlertCoordinator.clientIds =
      std::vector{PushNotificationClientId::kSafetyCheck};

  _optInAlertCoordinator.confirmationMessage = l10n_util::GetNSStringF(
      IDS_IOS_NOTIFICATIONS_CONFIRMATION_MESSAGE,
      l10n_util::GetStringUTF16(IDS_IOS_SAFETY_CHECK_TITLE));

  [_optInAlertCoordinator start];
}

// Opts the user out of Safety Check notifications and updates the push
// notification service preferences. Displays a confirmation snackbar with a
// link to notification settings.
- (void)disableNotifications {
  CHECK(IsSafetyCheckNotificationsEnabled());

  GetApplicationContext()->GetPushNotificationService()->SetPreference(
      nil, PushNotificationClientId::kSafetyCheck, false);

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

@end

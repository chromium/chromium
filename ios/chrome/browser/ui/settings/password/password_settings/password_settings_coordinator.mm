// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/debug/dump_without_crashing.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_bulk_move_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_export_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The user action for when the bulk move passwords to account confirmation
// dialog's cancel button is clicked.
constexpr const char* kBulkMovePasswordsToAccountConfirmationDialogCancelled =
    "Mobile.PasswordsSettings.BulkSavePasswordsToAccountDialog.Cancelled";

// The user action for when the bulk move passwords to account confirmation
// dialog's accept button is clicked.
constexpr const char* kBulkMovePasswordsToAccountConfirmationDialogAccepted =
    "Mobile.PasswordsSettings.BulkSavePasswordsToAccountDialog.Accepted";

}  // namespace

// Methods to update state in response to actions taken in the Export
// ActivityViewController.
@protocol ExportActivityViewControllerDelegate <NSObject>

// Used to reset the export state when the activity view disappears.
- (void)resetExport;

@end

// Convenience wrapper around ActivityViewController for presenting share sheet
// at the end of the export flow. We do not use completionWithItemsHandler
// because it fails sometimes; see crbug.com/820053.
@interface ExportActivityViewController : UIActivityViewController

- (instancetype)initWithActivityItems:(NSArray*)activityItems
                             delegate:(id<ExportActivityViewControllerDelegate>)
                                          delegate;

@end

@implementation ExportActivityViewController {
  __weak id<ExportActivityViewControllerDelegate> _weakDelegate;
}

- (instancetype)initWithActivityItems:(NSArray*)activityItems
                             delegate:(id<ExportActivityViewControllerDelegate>)
                                          delegate {
  self = [super initWithActivityItems:activityItems applicationActivities:nil];
  if (self) {
    _weakDelegate = delegate;
  }

  return self;
}

- (void)viewDidDisappear:(BOOL)animated {
  [_weakDelegate resetExport];
  [super viewDidDisappear:animated];
}

@end

@interface PasswordSettingsCoordinator () <
    ExportActivityViewControllerDelegate,
    BulkMoveLocalPasswordsToAccountHandler,
    PasswordExportHandler,
    PasswordSettingsPresentationDelegate,
    PasswordsInOtherAppsCoordinatorDelegate,
    PopoverLabelViewControllerDelegate,
    ReauthenticationCoordinatorDelegate,
    SettingsNavigationControllerDelegate>

@end

@implementation PasswordSettingsCoordinator {
  // Main view controller for this coordinator.
  PasswordSettingsViewController* _passwordSettingsViewController;

  // The presented SettingsNavigationController containing
  // `passwordSettingsViewController`.
  SettingsNavigationController* _settingsNavigationController;

  // The coupled mediator.
  PasswordSettingsMediator* _mediator;

  // Command dispatcher.
  __weak id<ApplicationCommands> _dispatcher;

  // Module handling reauthentication before accessing sensitive data.
  ReauthenticationModule* _reauthModule;

  // Coordinator for the "Passwords in Other Apps" screen.
  PasswordsInOtherAppsCoordinator* _passwordsInOtherAppsCoordinator;

  // Coordinator for blocking Password Settings until Local Authentication is
  // passed. Used for requiring authentication when opening Password Settings
  // from outside the Password Manager and when the app is
  // backgrounded/foregrounded with Password Settings opened.
  ReauthenticationCoordinator* _reauthCoordinator;

  // Service which gives us a view on users' saved passwords.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;

  // Alert informing the user that passwords are being prepared for
  // export.
  UIAlertController* _preparingPasswordsAlert;

  // For recording visits after successful authentication.
  IOSPasswordManagerVisitsRecorder* _visitsRecorder;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();

  _reauthModule = password_manager::BuildReauthenticationModule();

  _savedPasswordsPresenter =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          IOSChromeAffiliationServiceFactory::GetForProfile(profile),
          IOSChromeProfilePasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          IOSChromeAccountPasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          IOSPasskeyModelFactory::GetForProfile(profile));

  _mediator = [[PasswordSettingsMediator alloc]
         initWithReauthenticationModule:_reauthModule
                savedPasswordsPresenter:_savedPasswordsPresenter.get()
      bulkMovePasswordsToAccountHandler:self
                          exportHandler:self
                            prefService:profile->GetPrefs()
                        identityManager:IdentityManagerFactory::GetForProfile(
                                            profile)
                            syncService:SyncServiceFactory::GetForProfile(
                                            profile)];

  _dispatcher = static_cast<id<ApplicationCommands>>(
      self.browser->GetCommandDispatcher());

  _passwordSettingsViewController =
      [[PasswordSettingsViewController alloc] init];

  _passwordSettingsViewController.presentationDelegate = self;

  _settingsNavigationController = [[SettingsNavigationController alloc]
      initWithRootViewController:_passwordSettingsViewController
                         browser:self.browser
                        delegate:self];

  _mediator.consumer = _passwordSettingsViewController;
  _passwordSettingsViewController.delegate = _mediator;

  _visitsRecorder = [[IOSPasswordManagerVisitsRecorder alloc]
      initWithPasswordManagerSurface:password_manager::PasswordManagerSurface::
                                         kPasswordSettings];

  // Only record visit if no auth is required, otherwise wait for successful
  // auth.
  if (_skipAuthenticationOnStart) {
    [_visitsRecorder maybeRecordVisitMetric];
  }

  [self startReauthCoordinatorWithAuthOnStart:!_skipAuthenticationOnStart];

  [self.baseViewController presentViewController:_settingsNavigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self stopWithUIDismissal:YES];
}

#pragma mark - PasswordSettingsCoordinator

- (void)stopWithUIDismissal:(BOOL)shouldDismissUI {
  if (shouldDismissUI) {
    [_settingsNavigationController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }

  [_passwordsInOtherAppsCoordinator stop];
  _passwordsInOtherAppsCoordinator.delegate = nil;
  _passwordsInOtherAppsCoordinator = nil;

  _passwordSettingsViewController.presentationDelegate = nil;
  _passwordSettingsViewController.delegate = nil;
  _passwordSettingsViewController = nil;
  [_settingsNavigationController cleanUpSettings];
  _settingsNavigationController = nil;
  _preparingPasswordsAlert = nil;

  _dispatcher = nil;
  _reauthModule = nil;

  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  _savedPasswordsPresenter.reset();

  [self stopReauthenticationCoordinator];
}

#pragma mark - PasswordSettingsPresentationDelegate

- (void)startExportFlow {
  UIAlertController* exportConfirmation = [UIAlertController
      alertControllerWithTitle:nil
                       message:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_ALERT_MESSAGE)
                preferredStyle:UIAlertControllerStyleActionSheet];
  exportConfirmation.view.accessibilityIdentifier =
      kPasswordSettingsExportConfirmViewId;

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action){
                             }];
  [exportConfirmation addAction:cancelAction];

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* exportAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakSelf onStartExportFlowConfirmed];
              }];

  [exportConfirmation addAction:exportAction];

  exportConfirmation.popoverPresentationController.sourceView =
      [_passwordSettingsViewController sourceViewForAlerts];
  exportConfirmation.popoverPresentationController.sourceRect =
      [_passwordSettingsViewController sourceRectForPasswordExportAlerts];

  [_passwordSettingsViewController presentViewController:exportConfirmation
                                                animated:YES
                                              completion:nil];
}

- (void)showManagedPrefInfoForSourceView:(UIButton*)sourceView {
  // EnterpriseInfoPopoverViewController automatically handles reenabling the
  // `sourceView`, so we don't need to add any dismiss handlers or delegation,
  // just present the bubble.
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = sourceView;
  bubbleViewController.popoverPresentationController.sourceRect =
      sourceView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [_passwordSettingsViewController presentViewController:bubbleViewController
                                                animated:YES
                                              completion:nil];
}

- (void)showPasswordsInOtherAppsScreen {
  DCHECK(!_passwordsInOtherAppsCoordinator);
  [self stopReauthCoordinatorBeforeStartingChildCoordinator];
  _passwordsInOtherAppsCoordinator = [[PasswordsInOtherAppsCoordinator alloc]
      initWithBaseNavigationController:_settingsNavigationController
                               browser:self.browser];
  _passwordsInOtherAppsCoordinator.delegate = self;
  [_passwordsInOtherAppsCoordinator start];
}

- (void)showOnDeviceEncryptionSetUp {
  GURL URL = google_util::AppendGoogleLocaleParam(
      GURL(kOnDeviceEncryptionOptInURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [_dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)showOnDeviceEncryptionHelp {
  GURL URL = GURL(kOnDeviceEncryptionLearnMoreURL);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [_dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [_dispatcher
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:net::GURLWithNSURL(URL)
                                       inIncognito:NO]];
}

#pragma mark - BulkMoveLocalPasswordsToAccountHandler

- (void)showAuthenticationForMovePasswordsToAccountWithMessage:
    (NSString*)message {
  [_mediator userDidStartBulkMoveLocalPasswordsToAccountFlow];
}

- (void)showConfirmationDialogWithAlertTitle:(NSString*)alertTitle
                            alertDescription:(NSString*)alertDescription {
  // Create the confirmation alert.
  UIAlertController* movePasswordsConfirmation = [UIAlertController
      alertControllerWithTitle:alertTitle
                       message:alertDescription
                preferredStyle:UIAlertControllerStyleActionSheet];
  movePasswordsConfirmation.view.accessibilityIdentifier =
      kPasswordSettingsBulkMovePasswordsToAccountAlertViewId;

  // Create the cancel action.
  UIAlertAction* cancelAction = [UIAlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_ALERT_CANCEL)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    kBulkMovePasswordsToAccountConfirmationDialogCancelled));
              }];
  [movePasswordsConfirmation addAction:cancelAction];

  // Create the accept action (i.e. move passwords to account).
  __weak __typeof(self) weakSelf = self;
  UIAlertAction* movePasswordsAction = [UIAlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_ALERT_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    kBulkMovePasswordsToAccountConfirmationDialogAccepted));
                [weakSelf
                    showAuthenticationForMovePasswordsToAccountWithMessage:
                        alertTitle];
              }];

  [movePasswordsConfirmation addAction:movePasswordsAction];

  movePasswordsConfirmation.popoverPresentationController.sourceView =
      [_passwordSettingsViewController sourceViewForAlerts];
  movePasswordsConfirmation.popoverPresentationController.sourceRect =
      [_passwordSettingsViewController sourceRectForBulkMovePasswordsToAccount];

  // Show the alert.
  [_passwordSettingsViewController
      presentViewController:movePasswordsConfirmation
                   animated:YES
                 completion:nil];
}

- (void)showMovedToAccountSnackbarWithPasswordCount:(int)count
                                          userEmail:(std::string)email {
  std::u16string pattern = l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SNACKBAR_MESSAGE);
  std::u16string result = base::i18n::MessageFormatter::FormatWithNamedArgs(
      pattern, "COUNT", count, "EMAIL", base::UTF8ToUTF16(email));

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  id<SnackbarCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  [handler showSnackbarWithMessage:base::SysUTF16ToNSString(result)
                        buttonText:nil
                     messageAction:nil
                  completionAction:nil];
}

#pragma mark - PasswordExportHandler

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:(void (^)(NSString* activityType,
                                                    BOOL completed,
                                                    NSArray* returnedItems,
                                                    NSError* activityError))
                                              completionHandler {
  ExportActivityViewController* activityViewController =
      [[ExportActivityViewController alloc] initWithActivityItems:activityItems
                                                         delegate:self];
  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypeAirDrop,
    UIActivityTypeCopyToPasteboard, UIActivityTypeOpenInIBooks,
    UIActivityTypePostToFacebook, UIActivityTypePostToFlickr,
    UIActivityTypePostToTencentWeibo, UIActivityTypePostToTwitter,
    UIActivityTypePostToVimeo, UIActivityTypePostToWeibo, UIActivityTypePrint
  ];
  [activityViewController setExcludedActivityTypes:excludedActivityTypes];

  [activityViewController setCompletionWithItemsHandler:completionHandler];

  UIView* sourceView = [_passwordSettingsViewController sourceViewForAlerts];
  CGRect sourceRect =
      [_passwordSettingsViewController sourceRectForPasswordExportAlerts];

  activityViewController.modalPresentationStyle = UIModalPresentationPopover;
  activityViewController.popoverPresentationController.sourceView = sourceView;
  activityViewController.popoverPresentationController.sourceRect = sourceRect;
  activityViewController.popoverPresentationController
      .permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;

  [self presentViewControllerForExportFlow:activityViewController];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)localizedReason {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_FAILED_ALERT_TITLE)
                       message:localizedReason
                preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  [self presentViewControllerForExportFlow:alertController];
}

- (void)showPreparingPasswordsAlert {
  _preparingPasswordsAlert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_PREPARING_ALERT_TITLE)
                       message:nil
                preferredStyle:UIAlertControllerStyleAlert];
  __weak __typeof(self) weakSelf = self;
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction*) {
                               [weakSelf onExportFlowCancelled];
                             }];
  [_preparingPasswordsAlert addAction:cancelAction];
  [_passwordSettingsViewController
      presentViewController:_preparingPasswordsAlert
                   animated:YES
                 completion:nil];
}

- (void)showSetPasscodeForPasswordExportDialog {
  [self showSetPasscodeDialogWithContent:
            l10n_util::GetNSString(
                IDS_IOS_SETTINGS_EXPORT_PASSWORDS_SET_UP_SCREENLOCK_CONTENT)];
}

#pragma mark - ExportActivityViewControllerDelegate

- (void)resetExport {
  [_mediator userDidCompleteExportFlow];
}

#pragma mark - PasswordsInOtherAppsCoordinatorDelegate

- (void)passwordsInOtherAppsCoordinatorDidRemove:
    (PasswordsInOtherAppsCoordinator*)coordinator {
  DCHECK_EQ(_passwordsInOtherAppsCoordinator, coordinator);
  [_passwordsInOtherAppsCoordinator stop];
  _passwordsInOtherAppsCoordinator.delegate = nil;
  _passwordsInOtherAppsCoordinator = nil;
  [self restartReauthCoordinator];
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  // Dismiss UI and notify parent coordinator.
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:^{
                                                [weakSelf settingsWasDismissed];
                                              }];
}

- (void)settingsWasDismissed {
  [self.delegate passwordSettingsCoordinatorDidRemove:self];
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  [_visitsRecorder maybeRecordVisitMetric];
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)willPushReauthenticationViewController {
  // Cancel password export flow before authentication UI is presented.
  if (_preparingPasswordsAlert.beingPresented) {
    [_preparingPasswordsAlert dismissViewControllerAnimated:NO completion:nil];
    [_mediator exportFlowCanceled];
    _preparingPasswordsAlert = nil;
  }
}

#pragma mark - Private

// Closes the settings and load the passcode help article in a new tab.
- (void)showPasscodeHelp {
  GURL URL = GURL(kPasscodeArticleURL);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [_dispatcher closeSettingsUIAndOpenURL:command];
}

// Helper to show the "set passcode" dialog with customizable content.
- (void)showSetPasscodeDialogWithContent:(NSString*)content {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:content
                preferredStyle:UIAlertControllerStyleAlert];

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* learnAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [weakSelf showPasscodeHelp];
              }];
  [alertController addAction:learnAction];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;
  [_passwordSettingsViewController presentViewController:alertController
                                                animated:YES
                                              completion:nil];
}

// Helper method for presenting several ViewControllers used in the export flow.
// Ensures that the "Preparing passwords" alert is dismissed when something is
// ready to replace it.
- (void)presentViewControllerForExportFlow:(UIViewController*)viewController {
  if (_preparingPasswordsAlert.beingPresented) {
    __weak __typeof(self) weakSelf = self;
    [_preparingPasswordsAlert
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf presentViewControllerForExportFlow:
                                           viewController];
                           }];
  } else {
    [_passwordSettingsViewController presentViewController:viewController
                                                  animated:YES
                                                completion:nil];
  }
}

// Starts reauthCoordinator.
// - authOnStart: Pass `YES` to cover Password Settings with an empty view
// controller until successful Local Authentication when reauthCoordinator
// starts.
//
// Local authentication is required every time the current
// scene is backgrounded and foregrounded until reauthCoordinator is stopped.
- (void)startReauthCoordinatorWithAuthOnStart:(BOOL)authOnStart {
  if (_reauthCoordinator) {
    // The previous reauth coordinator should have been stopped and deallocated
    // by now. Create a crash report without crashing and gracefully handle the
    // error by cleaning up the old coordinator.
    base::debug::DumpWithoutCrashing();
    [_reauthCoordinator stopAndPopViewController];
  }

  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_settingsNavigationController
                               browser:self.browser
                reauthenticationModule:_reauthModule
                           authOnStart:authOnStart];

  _reauthCoordinator.delegate = self;

  [_reauthCoordinator start];
}

// Stops reauthCoordinator.
- (void)stopReauthenticationCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

// Stop reauth coordinator when a child coordinator will be started.
//
// Needed so reauth coordinator doesn't block for reauth if the scene state
// changes while the child coordinator is presenting its content. The child
// coordinator will add its own reauth coordinator to block its content for
// reauth.
- (void)stopReauthCoordinatorBeforeStartingChildCoordinator {
  // See PasswordsCoordinator
  // stopReauthCoordinatorBeforeStartingChildCoordinator.
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

// Starts the export passwords flow after the user confirmed the corresponding
// alert.
- (void)onStartExportFlowConfirmed {
  [_mediator userDidStartExportFlow];
}

// Cancels the password export flow.
- (void)onExportFlowCancelled {
  [_mediator exportFlowCanceled];
}

@end

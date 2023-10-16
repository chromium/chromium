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
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
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
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using password_manager::features::IsAuthOnEntryV2Enabled;

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
    SettingsNavigationControllerDelegate> {
  // Service which gives us a view on users' saved passwords.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;

  // Alert informing the user that passwords are being prepared for
  // export.
  UIAlertController* _preparingPasswordsAlert;
}

// Main view controller for this coordinator.
@property(nonatomic, strong)
    PasswordSettingsViewController* passwordSettingsViewController;

// The presented SettingsNavigationController containing
// `passwordSettingsViewController`.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// The coupled mediator.
@property(nonatomic, strong) PasswordSettingsMediator* mediator;

// Command dispatcher.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// Module handling reauthentication before accessing sensitive data.
@property(nonatomic, strong) ReauthenticationModule* reauthModule;

// Coordinator for the "Passwords in Other Apps" screen.
@property(nonatomic, strong)
    PasswordsInOtherAppsCoordinator* passwordsInOtherAppsCoordinator;

// Coordinator for blocking Password Settings until Local Authentication is
// passed. Used for requiring authentication when opening Password Settings
// from outside the Password Manager and when the app is
// backgrounded/foregrounded with Password Settings opened.
@property(nonatomic, strong) ReauthenticationCoordinator* reauthCoordinator;

@end

@implementation PasswordSettingsCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  self.reauthModule = password_manager::BuildReauthenticationModule();

  _savedPasswordsPresenter =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          IOSChromeAffiliationServiceFactory::GetForBrowserState(browserState),
          IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
              browserState, ServiceAccessType::EXPLICIT_ACCESS),
          IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
              browserState, ServiceAccessType::EXPLICIT_ACCESS));

  self.mediator = [[PasswordSettingsMediator alloc]
         initWithReauthenticationModule:self.reauthModule
                savedPasswordsPresenter:_savedPasswordsPresenter.get()
      bulkMovePasswordsToAccountHandler:self
                          exportHandler:self
                            prefService:browserState->GetPrefs()
                        identityManager:IdentityManagerFactory::
                                            GetForBrowserState(browserState)
                            syncService:SyncServiceFactory::GetForBrowserState(
                                            browserState)];

  self.dispatcher = static_cast<id<ApplicationCommands>>(
      self.browser->GetCommandDispatcher());

  self.passwordSettingsViewController =
      [[PasswordSettingsViewController alloc] init];

  self.passwordSettingsViewController.presentationDelegate = self;

  self.settingsNavigationController = [[SettingsNavigationController alloc]
      initWithRootViewController:self.passwordSettingsViewController
                         browser:self.browser
                        delegate:self];

  self.mediator.consumer = self.passwordSettingsViewController;
  self.passwordSettingsViewController.delegate = self.mediator;

  [self startReauthCoordinatorWithAuthOnStart:!_skipAuthenticationOnStart];

  [self.baseViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  // If the parent coordinator is stopping `self` while the UI is still being
  // presented, dismiss without animation. Dismissals due to user actions (e.g,
  // swipe or tap on Done) are animated.
  if (self.baseViewController.presentedViewController ==
      self.settingsNavigationController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }

  [self.passwordsInOtherAppsCoordinator stop];
  self.passwordsInOtherAppsCoordinator.delegate = nil;
  self.passwordsInOtherAppsCoordinator = nil;

  self.passwordSettingsViewController.presentationDelegate = nil;
  self.passwordSettingsViewController.delegate = nil;
  self.passwordSettingsViewController = nil;
  [self.settingsNavigationController cleanUpSettings];
  self.settingsNavigationController = nil;
  _preparingPasswordsAlert = nil;

  _dispatcher = nil;
  _reauthModule = nil;

  [self.mediator disconnect];
  self.mediator.consumer = nil;
  self.mediator = nil;
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

  __weak PasswordSettingsCoordinator* weakSelf = self;
  UIAlertAction* exportAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                PasswordSettingsCoordinator* strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                [strongSelf.mediator userDidStartExportFlow];
              }];

  [exportConfirmation addAction:exportAction];

  exportConfirmation.popoverPresentationController.sourceView =
      [self.passwordSettingsViewController sourceViewForAlerts];
  exportConfirmation.popoverPresentationController.sourceRect =
      [self.passwordSettingsViewController sourceRectForPasswordExportAlerts];

  [self.passwordSettingsViewController presentViewController:exportConfirmation
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

  [self.passwordSettingsViewController
      presentViewController:bubbleViewController
                   animated:YES
                 completion:nil];
}

- (void)showPasswordsInOtherAppsScreen {
  DCHECK(!self.passwordsInOtherAppsCoordinator);
  [self stopReauthCoordinatorBeforeStartingChildCoordinator];
  self.passwordsInOtherAppsCoordinator =
      [[PasswordsInOtherAppsCoordinator alloc]
          initWithBaseNavigationController:self.settingsNavigationController
                                   browser:self.browser];
  self.passwordsInOtherAppsCoordinator.delegate = self;
  [self.passwordsInOtherAppsCoordinator start];
}

- (void)showOnDeviceEncryptionSetUp {
  GURL URL = google_util::AppendGoogleLocaleParam(
      GURL(kOnDeviceEncryptionOptInURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)showOnDeviceEncryptionHelp {
  GURL URL = GURL(kOnDeviceEncryptionLearnMoreURL);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self.dispatcher
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:net::GURLWithNSURL(URL)
                                       inIncognito:NO]];
}

#pragma mark - BulkMoveLocalPasswordsToAccountHandler

- (void)showAuthenticationForMovePasswordsToAccountWithMessage:
    (NSString*)message {
  // No need to auth if AuthOnEntryV2 is enabled, since user is presumed to have
  // just recently authed.
  if (IsAuthOnEntryV2Enabled()) {
    [self.mediator userDidStartBulkMoveLocalPasswordsToAccountFlow];
    return;
  }

  if ([self.reauthModule canAttemptReauth]) {
    __weak PasswordSettingsCoordinator* weakSelf = self;

    void (^onReauthenticationFinished)(ReauthenticationResult) = ^(
        ReauthenticationResult result) {
      PasswordSettingsCoordinator* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }

      // On auth success, move passwords. Otherwise, do nothing.
      if (result == ReauthenticationResult::kSuccess) {
        [strongSelf.mediator userDidStartBulkMoveLocalPasswordsToAccountFlow];
      }
    };

    [self.reauthModule
        attemptReauthWithLocalizedReason:message
                    canReusePreviousAuth:NO
                                 handler:onReauthenticationFinished];
  } else {
    [self showSetPasscodeForMovePasswordsToAccountDialog];
  }
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
  __weak PasswordSettingsCoordinator* weakSelf = self;
  UIAlertAction* movePasswordsAction = [UIAlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_ALERT_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    kBulkMovePasswordsToAccountConfirmationDialogAccepted));
                PasswordSettingsCoordinator* strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                [strongSelf
                    showAuthenticationForMovePasswordsToAccountWithMessage:
                        alertTitle];
              }];

  [movePasswordsConfirmation addAction:movePasswordsAction];

  movePasswordsConfirmation.popoverPresentationController.sourceView =
      [self.passwordSettingsViewController sourceViewForAlerts];
  movePasswordsConfirmation.popoverPresentationController.sourceRect =
      [self.passwordSettingsViewController
              sourceRectForBulkMovePasswordsToAccount];

  // Show the alert.
  [self.passwordSettingsViewController
      presentViewController:movePasswordsConfirmation
                   animated:YES
                 completion:nil];
}

- (void)showSetPasscodeForMovePasswordsToAccountDialog {
  [self
      showSetPasscodeDialogWithContent:
          l10n_util::GetNSString(
              IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SET_UP_SCREENLOCK_CONTENT)];
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

  UIView* sourceView =
      [self.passwordSettingsViewController sourceViewForAlerts];
  CGRect sourceRect =
      [self.passwordSettingsViewController sourceRectForPasswordExportAlerts];

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
  __weak PasswordSettingsCoordinator* weakSelf = self;
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction*) {
                               [weakSelf.mediator exportFlowCanceled];
                             }];
  [_preparingPasswordsAlert addAction:cancelAction];
  [self.passwordSettingsViewController
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
  [self.mediator userDidCompleteExportFlow];
}

#pragma mark - PasswordsInOtherAppsCoordinatorDelegate

- (void)passwordsInOtherAppsCoordinatorDidRemove:
    (PasswordsInOtherAppsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordsInOtherAppsCoordinator, coordinator);
  [self.passwordsInOtherAppsCoordinator stop];
  self.passwordsInOtherAppsCoordinator.delegate = nil;
  self.passwordsInOtherAppsCoordinator = nil;
  [self restartReauthCoordinator];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  // Dismiss UI and notify parent coordinator.
  auto* __weak weakSelf = self;
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
  // No-op.
}

- (void)willPushReauthenticationViewController {
  // Cancel password export flow before authentication UI is presented.
  if (_preparingPasswordsAlert.beingPresented) {
    [_preparingPasswordsAlert dismissViewControllerAnimated:NO completion:nil];
    [self.mediator exportFlowCanceled];
    _preparingPasswordsAlert = nil;
  }
}

#pragma mark - Private

// Closes the settings and load the passcode help article in a new tab.
- (void)showPasscodeHelp {
  GURL URL = GURL(kPasscodeArticleURL);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

// Helper to show the "set passcode" dialog with customizable content.
- (void)showSetPasscodeDialogWithContent:(NSString*)content {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:content
                preferredStyle:UIAlertControllerStyleAlert];

  __weak PasswordSettingsCoordinator* weakSelf = self;
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
  [self.passwordSettingsViewController presentViewController:alertController
                                                    animated:YES
                                                  completion:nil];
}

// Helper method for presenting several ViewControllers used in the export flow.
// Ensures that the "Preparing passwords" alert is dismissed when something is
// ready to replace it.
- (void)presentViewControllerForExportFlow:(UIViewController*)viewController {
  if (_preparingPasswordsAlert.beingPresented) {
    __weak PasswordSettingsCoordinator* weakSelf = self;
    [_preparingPasswordsAlert
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf.passwordSettingsViewController
                                 presentViewController:viewController
                                              animated:YES
                                            completion:nil];
                           }];
  } else {
    [self.passwordSettingsViewController presentViewController:viewController
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
  // No-op if Auth on Entry is not enabled for the password manager.
  if (!IsAuthOnEntryV2Enabled()) {
    return;
  }

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
  if (IsAuthOnEntryV2Enabled()) {
    [self startReauthCoordinatorWithAuthOnStart:NO];
  }
}

@end

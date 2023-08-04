// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#import <utility>
#import <vector>

#import "base/mac/foundation_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordDetailsCoordinator () <PasswordDetailsHandler,
                                          PasswordDetailsMediatorDelegate> {
  password_manager::AffiliatedGroup _affiliatedGroup;
  password_manager::CredentialUIEntry _credential;

  // The context in which the password details are accessed.
  DetailsContext _context;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordDetailsTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordDetailsMediator* mediator;

// Module containing the reauthentication mechanism for viewing and copying
// passwords.
// Has to be strong for password bottom sheet feature or else it becomes nil.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Modal alert for interactions with password.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

@end

@implementation PasswordDetailsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          credential:
                              (const password_manager::CredentialUIEntry&)
                                  credential
                        reauthModule:(ReauthenticationModule*)reauthModule
                             context:(DetailsContext)context {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(navigationController);

    _baseNavigationController = navigationController;
    _credential = credential;
    _reauthenticationModule = reauthModule;
    _context = context;
  }
  return self;
}

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                     affiliatedGroup:(const password_manager::AffiliatedGroup&)
                                         affiliatedGroup
                        reauthModule:(ReauthenticationModule*)reauthModule
                             context:(DetailsContext)context {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(navigationController);

    _baseNavigationController = navigationController;
    _affiliatedGroup = affiliatedGroup;
    _reauthenticationModule = reauthModule;
    _context = context;
  }
  return self;
}

- (void)start {
  self.viewController = [[PasswordDetailsTableViewController alloc] init];

  std::vector<password_manager::CredentialUIEntry> credentials;
  NSString* displayName;
  if (_affiliatedGroup.GetCredentials().size() > 0) {
    displayName = [NSString
        stringWithUTF8String:_affiliatedGroup.GetDisplayName().c_str()];
    for (const auto& credentialGroup : _affiliatedGroup.GetCredentials()) {
      credentials.push_back(credentialGroup);
    }
  } else {
    credentials.push_back(_credential);
  }

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.mediator = [[PasswordDetailsMediator alloc]
         initWithPasswords:credentials
               displayName:displayName
      passwordCheckManager:IOSChromePasswordCheckManagerFactory::
                               GetForBrowserState(browserState)
                                   .get()
               prefService:browserState->GetPrefs()
               syncService:SyncServiceFactory::GetForBrowserState(browserState)
                   context:_context
                  delegate:self];
  self.mediator.consumer = self.viewController;
  self.viewController.handler = self;
  self.viewController.delegate = self.mediator;
  self.viewController.applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  self.viewController.snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  self.viewController.reauthModule = self.reauthenticationModule;
  if (self.showCancelButton) {
    [self.viewController setupLeftCancelButton];
  }
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - PasswordDetailsHandler

- (void)passwordDetailsTableViewControllerWasDismissed {
  [self.delegate passwordDetailsCoordinatorDidRemove:self];
}

- (void)dismissPasswordDetailsTableViewController {
  [self.delegate passwordDetailsCancelButtonWasTapped];
  [self.delegate passwordDetailsCoordinatorDidRemove:self];
}

- (void)showPasscodeDialogForReason:(PasscodeDialogReason)reason {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  NSString* message = l10n_util::GetNSString(
      reason == PasscodeDialogReasonShowPassword
          ? IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT
          : IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT_FOR_MOVE_TO_ACCOUNT);
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
                  id<ApplicationCommands> applicationCommandsHandler =
                      HandlerForProtocol(
                          weakSelf.browser->GetCommandDispatcher(),
                          ApplicationCommands);
                  [applicationCommandsHandler
                      closeSettingsUIAndOpenURL:command];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

- (void)showPasswordEditDialogWithOrigin:(NSString*)origin {
  NSString* message = l10n_util::GetNSStringF(IDS_IOS_EDIT_PASSWORD_DESCRIPTION,
                                              base::SysNSStringToUTF16(origin));
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:nil
                         message:message
                   barButtonItem:self.viewController.navigationItem
                                     .rightBarButtonItem];

  __weak __typeof(self) weakSelf = self;

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_EDIT)
                action:^{
                  [weakSelf.viewController passwordEditingConfirmed];
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_EDIT)
                action:nil
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

// TODO(crbug.com/1359392): By convention, passing nil for `anchorView` means
// to use the delete button in the bottom bar as the anchor. This is a temporary
// hack and will be removed when `kPasswordsGrouping` is enabled by default.
- (void)showPasswordDeleteDialogWithPasswordDetails:(PasswordDetails*)password
                                         anchorView:(UIView*)anchorView {
  NSString* title;
  NSString* message;
  // Blocked websites have empty `password` and no title or message.
  if ([password.password length]) {
    if (base::FeatureList::IsEnabled(
            password_manager::features::kPasswordsGrouping)) {
      std::tie(title, message) =
          GetPasswordAlertTitleAndMessageForOrigins(password.origins);
    } else {
      message = l10n_util::GetNSStringF(
          password.isCompromised
              ? IDS_IOS_DELETE_COMPROMISED_PASSWORD_DESCRIPTION
              : IDS_IOS_DELETE_PASSWORD_DESCRIPTION,
          base::SysNSStringToUTF16(password.origins[0]));
    }
  }
  NSString* buttonText =
      l10n_util::GetNSString(base::FeatureList::IsEnabled(
                                 password_manager::features::kPasswordsGrouping)
                                 ? IDS_IOS_DELETE_ACTION_TITLE
                                 : IDS_IOS_CONFIRM_PASSWORD_DELETION);

  self.actionSheetCoordinator =
      anchorView
          ? [[ActionSheetCoordinator alloc]
                initWithBaseViewController:self.viewController
                                   browser:self.browser
                                     title:title
                                   message:message
                                      rect:anchorView.bounds
                                      view:anchorView]
          : [[ActionSheetCoordinator alloc]
                initWithBaseViewController:self.viewController
                                   browser:self.browser
                                     title:title
                                   message:message
                             barButtonItem:self.viewController.deleteButton];
  __weak __typeof(self.mediator) weakMediator = self.mediator;
  [self.actionSheetCoordinator
      addItemWithTitle:buttonText
                action:^{
                  [weakMediator removeCredential:password];
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_DELETION)
                action:nil
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

- (void)moveCredentialToAccountStore:(PasswordDetails*)password
                          anchorView:(UIView*)anchorView
                     movedCompletion:(void (^)())movedCompletion {
  if (![self.mediator hasPasswordConflictInAccount:password]) {
    [self.mediator moveCredentialToAccountStore:password];
    movedCompletion();
    return;
  }
  NSString* actionSheetTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MOVE_CONFLICT_ACTION_SHEET_TITLE);
  NSString* actionSheetMessage = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_MOVE_CONFLICT_ACTION_SHEET_MESSAGE);
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:actionSheetTitle
                         message:actionSheetMessage
                            rect:anchorView.bounds
                            view:anchorView];

  __weak __typeof(self) weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_KEEP_RECENT_PASSWORD)
                action:^{
                  [weakSelf.mediator
                      moveCredentialToAccountStoreWithConflict:password];
                  movedCompletion();
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_MOVE)
                action:nil
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

- (void)showPasswordDetailsInEditModeWithoutAuthentication {
  [self.viewController showEditViewWithoutAuthentication];
}

- (void)onPasswordCopiedByUser {
  if (IsCredentialProviderExtensionPromoEnabledOnPasswordCopied()) {
    id<CredentialProviderPromoCommands> credentialProviderPromoHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(),
                           CredentialProviderPromoCommands);
    [credentialProviderPromoHandler
        showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                   PasswordCopied];
  }
}

- (void)onAllPasswordsDeleted {
  DCHECK_EQ(self.baseNavigationController.topViewController,
            self.viewController);
  // For password details opened outside of the settings context.
  if (_context == DetailsContext::kOutsideSettings) {
    [self dismissPasswordDetailsTableViewController];
  } else {
    // For password details opened from the Password Manager in the settings.
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
}

#pragma mark - PasswordDetailsMediatorDelegate

- (void)showDismissWarningDialogWithPasswordDetails:(PasswordDetails*)password {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_DISMISS_WARNING_DIALOG_TITLE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_DISMISS_WARNING_DIALOG_MESSAGE);
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];

  NSString* cancelButtonText = l10n_util::GetNSString(IDS_CANCEL);
  [self.alertCoordinator addItemWithTitle:cancelButtonText
                                   action:nil
                                    style:UIAlertActionStyleDefault];

  NSString* dismissButtonText =
      l10n_util::GetNSString(IDS_IOS_DISMISS_WARNING_DIALOG_DISMISS_BUTTON);
  __weak __typeof(self.mediator) weakMediator = self.mediator;
  [self.alertCoordinator
      addItemWithTitle:dismissButtonText
                action:^{
                  [weakMediator didConfirmWarningDismissalForPassword:password];
                }
                 style:UIAlertActionStyleDefault
             preferred:YES
               enabled:YES];
  [self.alertCoordinator start];
}

- (void)updateFormManagers {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!activeWebState) {
    // PasswordDetailsCoordinator and other settings coordinators always receive
    // a normal Browser, even if they are started from incognito. So if only
    // incognito tabs are open, `activeWebState` is null, causing a crash
    // (crbug.com/1468506).
    return;
  }
  password_manager::PasswordManagerClient* passwordManagerClient =
      PasswordTabHelper::FromWebState(activeWebState)
          ->GetPasswordManagerClient();
  passwordManagerClient->UpdateFormManagers();
}

@end

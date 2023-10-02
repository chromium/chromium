// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator+private.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::features::IsAuthOnEntryV2Enabled;

@interface PasswordDetailsCoordinator () <
    PasswordDetailsHandler,
    PasswordDetailsMediatorDelegate,
    ReauthenticationCoordinatorDelegate,
    PasswordSharingCoordinatorDelegate,
    PasswordSharingFirstRunCoordinatorDelegate> {
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

// Coordinator for the password sharing flow.
@property(nonatomic, strong)
    PasswordSharingCoordinator* passwordSharingCoordinator;

// Coordinator for the password sharing first run flow.
@property(nonatomic, strong)
    PasswordSharingFirstRunCoordinator* passwordSharingFirstRunCoordinator;

// Coordinator for blocking password details until Local Authentication is
// successful.
@property(nonatomic, strong) ReauthenticationCoordinator* reauthCoordinator;

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
  self.mediator =
      [[PasswordDetailsMediator alloc] initWithPasswords:credentials
                                             displayName:displayName
                                            browserState:browserState
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

  // Disable animation when content will be blocked for reauth to prevent
  // flickering in navigation bar.
  [self.baseNavigationController
      pushViewController:self.viewController
                animated:![self shouldRequireAuthOnStart]];
  if (IsAuthOnEntryV2Enabled()) {
    [self startReauthCoordinator];
  }
}

- (void)stop {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
  [self dismissActionSheetCoordinator];
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
  [self dismissAlertCoordinator];
}

#pragma mark - PasswordDetailsHandler

- (void)passwordDetailsTableViewControllerWasDismissed {
  [self.delegate passwordDetailsCoordinatorDidRemove:self];
}

- (void)dismissPasswordDetailsTableViewController {
  [self.delegate passwordDetailsCancelButtonWasTapped];
  [self.delegate passwordDetailsCoordinatorDidRemove:self];
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
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_EDIT)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

- (void)showPasswordDeleteDialogWithPasswordDetails:(PasswordDetails*)password
                                         anchorView:(UIView*)anchorView {
  NSString* title;
  NSString* message;
  // Blocked websites have empty `password` and no title or message.
  if ([password.password length]) {
    std::tie(title, message) =
        password_manager::GetPasswordAlertTitleAndMessageForOrigins(
            password.origins);
  }
  NSString* buttonText = l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE);

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
  __weak __typeof(self) weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:buttonText
                action:^{
                  [weakMediator removeCredential:password];
                  [weakSelf dismissActionSheetCoordinator];
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
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_MOVE)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
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

- (void)onShareButtonPressed {
  if (self.browser->GetBrowserState()->GetPrefs()->GetBoolean(
          prefs::kPasswordSharingFlowHasBeenEntered)) {
    [self startPasswordSharingCoordinator];
  } else {
    [self.passwordSharingFirstRunCoordinator stop];
    self.passwordSharingFirstRunCoordinator =
        [[PasswordSharingFirstRunCoordinator alloc]
            initWithBaseViewController:self.baseViewController
                               browser:self.browser];
    self.passwordSharingFirstRunCoordinator.delegate = self;
    [self.passwordSharingFirstRunCoordinator start];
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
  __weak PasswordDetailsCoordinator* weakSelf = self;
  [self.alertCoordinator addItemWithTitle:cancelButtonText
                                   action:^{
                                     [weakSelf dismissAlertCoordinator];
                                   }
                                    style:UIAlertActionStyleDefault];

  NSString* dismissButtonText =
      l10n_util::GetNSString(IDS_IOS_DISMISS_WARNING_DIALOG_DISMISS_BUTTON);
  __weak __typeof(self.mediator) weakMediator = self.mediator;
  [self.alertCoordinator
      addItemWithTitle:dismissButtonText
                action:^{
                  [weakMediator didConfirmWarningDismissalForPassword:password];
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault
             preferred:YES
               enabled:YES];
  [self.alertCoordinator start];
}

- (void)updateFormManagers {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);

  for (Browser* browser : browserList->AllRegularBrowsers()) {
    [self updateFormManagersForBrowser:browser];
  }

  for (Browser* browser : browserList->AllIncognitoBrowsers()) {
    [self updateFormManagersForBrowser:browser];
  }
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  // No-op.
}

- (void)willPushReauthenticationViewController {
  // Dismiss modal ui before reauth view controller is pushed in front of
  // password details.
  [self dismissAlertCoordinator];
  [self dismissActionSheetCoordinator];
  [self dismissPasswordSharingCoordinator];
}

#pragma mark - PasswordSharingCoordinatorDelegate

- (void)passwordSharingCoordinatorDidRemove:
    (PasswordSharingCoordinator*)coordinator {
  if (self.passwordSharingCoordinator == coordinator) {
    [self.passwordSharingCoordinator stop];
    self.passwordSharingCoordinator.delegate = nil;
    self.passwordSharingCoordinator = nil;
  }
}

#pragma mark - PasswordSharingFirstRunCoordinatorDelegate

- (void)passwordSharingFirstRunCoordinatorDidAccept:
    (PasswordSharingFirstRunCoordinator*)coordinator {
  self.browser->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kPasswordSharingFlowHasBeenEntered, true);

  if (self.passwordSharingFirstRunCoordinator == coordinator) {
    [self stopPasswordSharingFirstRunCoordinatorWithCompletion:^{
      [self startPasswordSharingCoordinator];
    }];
  }
}

- (void)passwordSharingFirstRunCoordinatorWasDismissed:
    (PasswordSharingFirstRunCoordinator*)coordinator {
  if (self.passwordSharingFirstRunCoordinator == coordinator) {
    [self stopPasswordSharingFirstRunCoordinatorWithCompletion:nil];
  }
}

#pragma mark - Private

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

- (void)dismissAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (void)dismissPasswordSharingCoordinator {
  [_passwordSharingCoordinator stop];
  _passwordSharingCoordinator = nil;
}

// Starts reauthCoordinator. If Password Details was opened from outside the
// Password Manager, Local Authentication is required. Once started
// reauthCoordinator observes scene state changes and requires authentication
// when the scene is backgrounded and then foregrounded while Password Details
// is opened.
- (void)startReauthCoordinator {
  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser
                reauthenticationModule:_reauthenticationModule
                           authOnStart:[self shouldRequireAuthOnStart]];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

// Starts the main coordinator for the password sharing flow.
- (void)startPasswordSharingCoordinator {
  [self.passwordSharingCoordinator stop];
  self.passwordSharingCoordinator = [[PasswordSharingCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                     credentials:self.mediator.credentials
         savedPasswordsPresenter:self.mediator.savedPasswordsPresenter];
  self.passwordSharingCoordinator.delegate = self;
  [self.passwordSharingCoordinator start];
}

// Stops the first run coordinator for the password sharing flow and calls
// `completion` on its vc dismissal.
- (void)stopPasswordSharingFirstRunCoordinatorWithCompletion:
    (ProceduralBlock)completion {
  [self.passwordSharingFirstRunCoordinator stopWithCompletion:completion];
  self.passwordSharingFirstRunCoordinator.delegate = nil;
  self.passwordSharingFirstRunCoordinator = nil;
}

// Whether Local Authentication should be required before displaying the
// contents of Password Details.
- (BOOL)shouldRequireAuthOnStart {
  if (!IsAuthOnEntryV2Enabled()) {
    return NO;
  }

  // Authentication required only if opening Password Details from outside the
  // Password Manager.
  switch (_context) {
    case DetailsContext::kWeakIssues:
    case DetailsContext::kReusedIssues:
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kDismissedWarnings:
      return NO;
    case DetailsContext::kOutsideSettings:
      return YES;
  }
}

// Refreshes the password suggestions list for a specific `browser`.
- (void)updateFormManagersForBrowser:(Browser*)browser {
  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return;
  }
  password_manager::PasswordManagerClient* passwordManagerClient =
      PasswordTabHelper::FromWebState(webState)->GetPasswordManagerClient();
  passwordManagerClient->UpdateFormManagers();
}

@end

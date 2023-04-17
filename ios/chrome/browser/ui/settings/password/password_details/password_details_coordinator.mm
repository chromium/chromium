// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#import <set>
#import <utility>
#import <vector>

#import "base/mac/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class PasswordManagerClientProviderImpl : public PasswordManagerClientProvider {
 public:
  explicit PasswordManagerClientProviderImpl(Browser* browser)
      : browser_(browser) {}

  ~PasswordManagerClientProviderImpl() override = default;

  PasswordManagerClientProviderImpl(const PasswordManagerClientProviderImpl&) =
      delete;
  PasswordManagerClientProviderImpl& operator=(
      const PasswordManagerClientProviderImpl&) = delete;

  password_manager::PasswordManagerClient* GetAny() override {
    web::WebState* active_tab_in_browser =
        browser_->GetWebStateList()->GetActiveWebState();
    if (active_tab_in_browser) {
      return PasswordTabHelper::FromWebState(active_tab_in_browser)
          ->GetPasswordManagerClient();
    }

    // PasswordDetailsCoordinator and other settings coordinators always receive
    // a normal Browser, even if they are started from incognito. So if only
    // incognito tabs are open, `active_tab_in_browser` is null, causing a crash
    // (crbug.com/1431975).
    // In that case, use an open tab in any Browser. It doesn't matter which
    // one. This is a sad workaround for the fact that some PasswordManager
    // layers depend on tabs unnecessarily.
    BrowserList* browser_list =
        BrowserListFactory::GetForBrowserState(browser_->GetBrowserState());
    for (const std::set<Browser*>& browsers :
         {browser_list->AllRegularBrowsers(),
          browser_list->AllIncognitoBrowsers()}) {
      for (Browser* other_browser : browsers) {
        web::WebState* other_active_tab =
            other_browser->GetWebStateList()->GetActiveWebState();
        if (other_active_tab) {
          return PasswordTabHelper::FromWebState(other_active_tab)
              ->GetPasswordManagerClient();
        }
      }
    }

    // It's impossible to open PasswordDetailsCoordinator without an open tab.
    NOTREACHED_NORETURN();
  }

 private:
  const raw_ptr<Browser> browser_;
};

}  // namespace

@interface PasswordDetailsCoordinator () <PasswordDetailsHandler> {
  password_manager::AffiliatedGroup _affiliatedGroup;
  password_manager::CredentialUIEntry _credential;

  // Tells whether or not to support move to account option. If YES, move option
  // will be supported, NO otherwise.
  BOOL _supportMoveToAccount;

  // See PasswordManagerClientProviderImpl docs.
  std::unique_ptr<PasswordManagerClientProviderImpl>
      _passwordManagerClientProvider;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordDetailsTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordDetailsMediator* mediator;

// Module containing the reauthentication mechanism for viewing and copying
// passwords.
@property(nonatomic, weak) ReauthenticationModule* reauthenticationModule;

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
                supportMoveToAccount:(BOOL)supportMoveToAccount {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(navigationController);

    _baseNavigationController = navigationController;
    _credential = credential;
    _reauthenticationModule = reauthModule;
    _supportMoveToAccount = supportMoveToAccount;
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
                supportMoveToAccount:(BOOL)supportMoveToAccount {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(navigationController);

    _baseNavigationController = navigationController;
    _affiliatedGroup = affiliatedGroup;
    _reauthenticationModule = reauthModule;
    _supportMoveToAccount = supportMoveToAccount;
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
  _passwordManagerClientProvider =
      std::make_unique<PasswordManagerClientProviderImpl>(self.browser);
  self.mediator = [[PasswordDetailsMediator alloc]
                  initWithPasswords:credentials
                        displayName:displayName
               passwordCheckManager:IOSChromePasswordCheckManagerFactory::
                                        GetForBrowserState(browserState)
                                            .get()
                        prefService:browserState->GetPrefs()
                        syncService:SyncServiceFactory::GetForBrowserState(
                                        browserState)
               supportMoveToAccount:_supportMoveToAccount
      passwordManagerClientProvider:_passwordManagerClientProvider.get()];
  self.mediator.consumer = self.viewController;
  self.viewController.handler = self;
  self.viewController.delegate = self.mediator;
  self.viewController.applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  self.viewController.snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  self.viewController.reauthModule = self.reauthenticationModule;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - PasswordDetailsHandler

- (void)passwordDetailsTableViewControllerDidDisappear {
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
  [self.baseNavigationController popViewControllerAnimated:YES];
}

@end

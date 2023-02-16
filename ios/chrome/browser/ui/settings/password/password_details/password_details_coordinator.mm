// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#import <utility>
#import <vector>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordDetailsCoordinator () <PasswordDetailsHandler> {
  password_manager::AffiliatedGroup _affiliatedGroup;
  password_manager::CredentialUIEntry _credential;

  // Manager responsible for password check feature.
  IOSChromePasswordCheckManager* _manager;

  // The handler used for CredentialProviderPromoCommands.
  id<CredentialProviderPromoCommands> _credentialProviderPromoHandler;
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
                passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(navigationController);
    DCHECK(manager);

    _baseNavigationController = navigationController;
    _credential = credential;
    _manager = manager;
    _reauthenticationModule = reauthModule;
    if (IsCredentialProviderExtensionPromoEnabled()) {
      _credentialProviderPromoHandler = HandlerForProtocol(
          browser->GetCommandDispatcher(), CredentialProviderPromoCommands);
    }
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
                passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(navigationController);
    DCHECK(manager);

    _baseNavigationController = navigationController;
    _affiliatedGroup = affiliatedGroup;
    _manager = manager;
    _reauthenticationModule = reauthModule;
    if (IsCredentialProviderExtensionPromoEnabled()) {
      _credentialProviderPromoHandler = HandlerForProtocol(
          browser->GetCommandDispatcher(), CredentialProviderPromoCommands);
    }
  }
  return self;
}

- (void)start {
  self.viewController =
      [[PasswordDetailsTableViewController alloc] initWithSyncingUserEmail:nil];

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

  self.mediator = [[PasswordDetailsMediator alloc] initWithPasswords:credentials
                                                         displayName:displayName
                                                passwordCheckManager:_manager];
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

- (void)showPasscodeDialog {
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
                                      rect:anchorView.frame
                                      view:anchorView]
          : [[ActionSheetCoordinator alloc]
                initWithBaseViewController:self.viewController
                                   browser:self.browser
                                     title:title
                                   message:message
                             barButtonItem:self.viewController.deleteButton];
  __weak __typeof(self) weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:buttonText
                action:^{
                  [weakSelf
                      passwordDeletionConfirmedForCompromised:password
                                                                  .isCompromised
                                                     password:password];
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_DELETION)
                action:nil
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

- (void)showPasswordDetailsInEditModeWithoutAuthentication {
  [self.viewController showEditViewWithoutAuthentication];
}

- (void)removeCredentialFromCacheAndRefreshTableView:
    (const password_manager::CredentialUIEntry&)credential {
  // Remove credential from the credentials cache of the password details
  // manager.
  [self.mediator removeCredential:credential];

  [self.mediator didFinishEditingPasswordDetails];
}

- (void)onPasswordCopiedByUser {
  if (IsCredentialProviderExtensionPromoEnabled()) {
    DCHECK(_credentialProviderPromoHandler);
    [_credentialProviderPromoHandler
        showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                   PasswordCopied];
  }
}

#pragma mark - Private

// Notifies delegate about password deletion and records metric if needed.
- (void)passwordDeletionConfirmedForCompromised:(BOOL)compromised
                                       password:(PasswordDetails*)password {
  // Map from PasswordDetails to CredentialUIEntry. Should support blocklists.
  // `self.mediator.credentials` returns a different copy on each call, so cache
  // in a single local variable for use below.
  std::vector<password_manager::CredentialUIEntry> credentials =
      self.mediator.credentials;
  auto it = base::ranges::find_if(
      credentials,
      [password](const password_manager::CredentialUIEntry& credential) {
        return credential.GetFirstSignonRealm() ==
                   base::SysNSStringToUTF8(password.signonRealm) &&
               credential.username ==
                   base::SysNSStringToUTF16(password.username) &&
               credential.password ==
                   base::SysNSStringToUTF16(password.password);
      });
  if (it == credentials.end()) {
    // TODO(crbug.com/1359392): Convert into DCHECK.
    return;
  }

  [self.delegate passwordDetailsCoordinator:self
                           deleteCredential:*it
                          shouldDismissView:(credentials.size() - 1 == 0)];
  if (compromised) {
    base::UmaHistogramEnumeration(
        "PasswordManager.BulkCheck.UserAction",
        password_manager::metrics_util::PasswordCheckInteraction::
            kRemovePassword);
  }
}

@end

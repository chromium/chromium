// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/legacy_accounts_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/identity_view_item.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_model_identity_data_source.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_view_controlling.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;
using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

namespace {

// The size of the symbol image.
const CGFloat kSymbolAddAccountPointSize = 20;

typedef NS_ENUM(NSInteger, AccountsSectionIdentifier) {
  SectionIdentifierAccounts = kSectionIdentifierEnumZero,
  SectionIdentifierSignOut,
};

typedef NS_ENUM(NSInteger, AccountsItemType) {
  ItemTypeAccount = kItemTypeEnumZero,
  // Sign in item.
  ItemTypeSignInHeader,
  ItemTypeAddAccount,
  // Indicates that restricted accounts are removed from the list.
  ItemTypeRestrictedAccountsFooter,
  // Provides sign out items used only for non-managed accounts.
  ItemTypeSignOut,
  // Detailed description of the actions taken by sign out, e.g. turning off
  // sync.
  ItemTypeSignOutSyncingFooter,
};

}  // namespace

@interface LegacyAccountsTableViewController () <
    SignoutActionSheetCoordinatorDelegate> {
  raw_ptr<Browser> _browser;
  BOOL _closeSettingsOnAddAccount;
  // Whether an authentication operation is in progress (e.g switch accounts,
  // sign out).
  BOOL _authenticationOperationInProgress;
  // Whether the view controller is currently being dismissed and new dismiss
  // requests should be ignored.
  BOOL _isBeingDismissed;

  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, TableViewItem*>* _identityMap;

  // ApplicationCommands handler.
  id<ApplicationCommands> _applicationHandler;

  // If YES, ManageAccountsTableViewController should not dismiss itself only
  // for a sign-out reason. The parent coordinator is responsible to dismiss
  // this coordinator when a sign-out happens.
  BOOL _signoutDismissalByParentCoordinator;
  SigninCoordinator* _signinCoordinator;
}

// Modal alert to choose between remove an identity and show MyGoogle UI.
@property(nonatomic, strong)
    AlertCoordinator* removeOrMyGoogleChooserAlertCoordinator;

// Modal alert for confirming account removal.
@property(nonatomic, strong) AlertCoordinator* removeAccountCoordinator;

// Modal alert for sign out.
@property(nonatomic, strong) SignoutActionSheetCoordinator* signoutCoordinator;

// If YES, the UI elements are disabled.
@property(nonatomic, assign) BOOL uiDisabled;

@end

@implementation LegacyAccountsTableViewController {
  // Callback to dismiss MyGoogle (Account Detail).
  DismissViewCallback _accountDetailsControllerDismissCallback;
}

@synthesize modelIdentityDataSource;

- (instancetype)initWithBrowser:(Browser*)browser
              closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount
             applicationCommandsHandler:
                 (id<ApplicationCommands>)applicationCommandsHandler
    signoutDismissalByParentCoordinator:
        (BOOL)signoutDismissalByParentCoordinator {
  DCHECK(browser);
  DCHECK(!browser->GetProfile()->IsOffTheRecord());

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _applicationHandler = applicationCommandsHandler;
    _signoutDismissalByParentCoordinator = signoutDismissalByParentCoordinator;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kSettingsLegacyAccountsTableViewId;

  [self loadModel];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAccountsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAccountsSettingsBack"));
}

- (void)settingsWillBeDismissed {
  if (!_accountDetailsControllerDismissCallback.is_null()) {
    DCHECK(self.presentedViewController);
    DCHECK(!self.signoutCoordinator);
    DCHECK(!self.removeOrMyGoogleChooserAlertCoordinator);
    DCHECK(!self.removeAccountCoordinator);
    std::move(_accountDetailsControllerDismissCallback).Run(/*animated=*/false);
  }
  [self dismissRemoveOrMyGoogleChooserAlert];
  [self.signoutCoordinator stop];
  self.signoutCoordinator = nil;
  [self dismissRemoveAccountCoordinator];
  [self stopSigninCoordinator];
  _browser = nullptr;

  _isBeingDismissed = YES;
}

#pragma mark - SettingsRootTableViewController

- (void)reloadData {
  if (!_browser) {
    return;
  }

  if (![self authService]->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // This accounts table view will be popped or dismissed when the user
    // is signed out. Avoid reloading it in that case as that would lead to an
    // empty table view.
    return;
  }
  [super reloadData];
}

- (void)loadModel {
  if (!_browser) {
    return;
  }

  // Update the title with the name with the currently signed-in account.
  AuthenticationService* authService = self.authService;
  id<SystemIdentity> authenticatedIdentity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  self.title = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_ACCOUNTS_MANAGEMENT_FROM_ACCOUNT_SETTINGS_TITLE);

  [super loadModel];

  if (!authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  NSMutableDictionary<NSString*, TableViewItem*>* mutableIdentityMap =
      [[NSMutableDictionary alloc] init];

  // Account cells.
  [model addSectionWithIdentifier:SectionIdentifierAccounts];
  [model setHeader:[self signInHeader]
      forSectionWithIdentifier:SectionIdentifierAccounts];

  NSString* authenticatedEmail = authenticatedIdentity.userEmail;
  for (const auto& identityViewItem :
       [self.modelIdentityDataSource identityViewItems]) {
    if (!identityViewItem) {
      // Ignore the case in which the identity is invalid at lookup time. This
      // may be due to inconsistencies between the identity service and
      // ProfileOAuth2TokenService.
      continue;
    }
    // TODO(crbug.com/40691260): This re-ordering will be redundant once we
    // apply ordering changes to the account reconciler.
    TableViewItem* item =
        [self accountItemWithIdentityViewItem:identityViewItem];
    if ([identityViewItem.userEmail isEqualToString:authenticatedEmail]) {
      [model insertItem:item
          inSectionWithIdentifier:SectionIdentifierAccounts
                          atIndex:0];
    } else {
      [model addItem:item toSectionWithIdentifier:SectionIdentifierAccounts];
    }

    [mutableIdentityMap setObject:item forKey:identityViewItem.gaiaID];
  }
  _identityMap = mutableIdentityMap;

  [model addItem:[self addAccountItem]
      toSectionWithIdentifier:SectionIdentifierAccounts];

  if (IsRestrictAccountsToPatternsEnabled()) {
    [model setFooter:[self restrictedIdentitiesFooterItem]
        forSectionWithIdentifier:SectionIdentifierAccounts];
  }

  // Sign out section.
  [model addSectionWithIdentifier:SectionIdentifierSignOut];
  [model addItem:[self signOutItem]
      toSectionWithIdentifier:SectionIdentifierSignOut];

  if ([self authService]->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy) {
    TableViewLinkHeaderFooterItem* footerItem =
        [self signOutSyncingFooterItemForForcedSignin];
    [model setFooter:footerItem
        forSectionWithIdentifier:SectionIdentifierSignOut];
  }
}

#pragma mark - Model objects

- (TableViewTextHeaderFooterItem*)signInHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeSignInHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_DESCRIPTION);
  return header;
}

- (TableViewLinkHeaderFooterItem*)signOutSyncingFooterItemForForcedSignin {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutSyncingFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE);

  footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(kChromeUIManagementURL)] ];
  return footer;
}

- (TableViewLinkHeaderFooterItem*)restrictedIdentitiesFooterItem {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeRestrictedAccountsFooter];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_RESTRICTED_IDENTITIES);
  footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(kChromeUIManagementURL)] ];
  return footer;
}

- (TableViewItem*)accountItemWithIdentityViewItem:
    (IdentityViewItem*)identityViewItem {
  TableViewAccountItem* item =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  [self updateAccountItem:item withIdentityViewItem:identityViewItem];
  return item;
}

- (void)updateAccountItem:(TableViewAccountItem*)item
     withIdentityViewItem:(IdentityViewItem*)identityViewItem {
  item.image = identityViewItem.avatar;
  item.text = identityViewItem.userEmail;
  item.identity =
      [self.modelIdentityDataSource identityWithGaiaID:identityViewItem.gaiaID];
  item.accessibilityIdentifier = identityViewItem.accessibilityIdentifier;
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
}

- (TableViewItem*)addAccountItem {
  TableViewAccountItem* item =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAddAccount];
  item.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_ADD_ACCOUNT_BUTTON);
  item.accessibilityIdentifier = kSettingsAccountsTableViewAddAccountCellId;
  item.image = CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                         kSymbolAddAccountPointSize);
  return item;
}

- (TableViewItem*)signOutItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeSignOut];
  item.text =
      l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSettingsAccountsTableViewSignoutCellId;
  return item;
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case SectionIdentifierAccounts:
    case SectionIdentifierSignOut: {
      // Might be a different type of footer.
      TableViewLinkHeaderFooterView* linkView =
          base::apple::ObjCCast<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
      break;
    }
  }
  return view;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // If there is an operation in process that does not allow selecting a cell or
  // if the settings will be dismissed, exit without performing the selection.
  if (self.uiDisabled || _isBeingDismissed) {
    return;
  }

  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  AccountsItemType itemType = static_cast<AccountsItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  switch (itemType) {
    case ItemTypeAccount: {
      base::RecordAction(
          base::UserMetricsAction("Signin_AccountsTableView_AccountDetail"));
      TableViewAccountItem* item =
          base::apple::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      DCHECK(item.identity);

      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showAccountDetails:item.identity itemView:itemView];
      break;
    }
    case ItemTypeAddAccount: {
      base::RecordAction(
          base::UserMetricsAction("Signin_AccountsTableView_AddAccount"));
      [self showAddAccount];
      break;
    }
    case ItemTypeSignOut: {
      base::RecordAction(
          base::UserMetricsAction("Signin_AccountsTableView_SignOut"));
      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showSignOutWithItemView:itemView];
      break;
    }
    case ItemTypeSignInHeader:
    case ItemTypeSignOutSyncingFooter:
    case ItemTypeRestrictedAccountsFooter:
      NOTREACHED();
  }

  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Authentication operations

- (void)showAddAccount {
  DCHECK(!self.removeOrMyGoogleChooserAlertCoordinator);
  _authenticationOperationInProgress = YES;

  // TODO(crbug.com/40229802): Remove the following line when todo bug will be
  // fixed.
  [self preventUserInteraction];
  __weak __typeof(self) weakSelf = self;
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;
  AccessPoint accessPoint = AccessPoint::kSettings;
  _signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self
                                          browser:_browser
                                     contextStyle:contextStyle
                                      accessPoint:accessPoint
                             continuationProvider:
                                 DoNothingContinuationProvider()];
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, id<SystemIdentity> completionIdentity) {
        [weakSelf handleDidAddAccount:result];
      };
  [_signinCoordinator start];
}

- (void)handleDidAddAccount:(SigninCoordinatorResult)result {
  // TODO(crbug.com/40229802): Remove the following line when todo bug will be
  // fixed.
  [self allowUserInteraction];
  [self stopSigninCoordinator];
  [self handleAuthenticationOperationDidFinish];
  if (result == SigninCoordinatorResult::SigninCoordinatorResultSuccess &&
      _closeSettingsOnAddAccount) {
    [_applicationHandler closePresentedViews];
  }
}

- (void)showAccountDetails:(id<SystemIdentity>)identity
                  itemView:(UIView*)itemView {
  if (self.removeOrMyGoogleChooserAlertCoordinator) {
    // It is possible for the user to tap twice on the cell. If the action
    // sheet coordinator already exists, we need to ignore the second tap.
    // Related to crbug.com/1497100.
    return;
  }
  self.removeOrMyGoogleChooserAlertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:nil
                         message:identity.userEmail
                            rect:itemView.frame
                            view:itemView];
  __weak __typeof(self) weakSelf = self;
  [self.removeOrMyGoogleChooserAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "Signin_AccountsTableView_AccountDetail_OpenMyGoogleUI"));
                  [weakSelf handleManageGoogleAccountWithIdentity:identity];
                  [weakSelf dismissRemoveOrMyGoogleChooserAlert];
                }
                 style:UIAlertActionStyleDefault];
  [self.removeOrMyGoogleChooserAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_REMOVE_GOOGLE_ACCOUNT_TITLE)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "Signin_AccountsTableView_AccountDetail_RemoveAccount"));
                  [weakSelf handleRemoveAccountWithIdentity:identity];
                  [weakSelf dismissRemoveOrMyGoogleChooserAlert];
                }
                 style:UIAlertActionStyleDestructive];
  [self.removeOrMyGoogleChooserAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^() {
                  base::RecordAction(base::UserMetricsAction(
                      "Signin_AccountsTableView_AccountDetail_Cancel"));
                  [weakSelf handleAlertCoordinatorCancel];
                  [weakSelf dismissRemoveOrMyGoogleChooserAlert];
                }
                 style:UIAlertActionStyleCancel];
  [self.removeOrMyGoogleChooserAlertCoordinator start];
}

// Handles the manage Google account action from
// `self.removeOrMyGoogleChooserAlertCoordinator`. Action sheet created in
// `showAccountDetails:itemView:`
- (void)handleManageGoogleAccountWithIdentity:(id<SystemIdentity>)identity {
  DCHECK(self.removeOrMyGoogleChooserAlertCoordinator);
  // `self.removeOrMyGoogleChooserAlertCoordinator` should not be stopped, since
  // the coordinator has been confirmed.
  self.removeOrMyGoogleChooserAlertCoordinator = nil;
  __strong __typeof(self) weakSelf = self;
  _accountDetailsControllerDismissCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentAccountDetailsController(
              identity, self,
              /*animated=*/YES,
              base::BindOnce(
                  [](__typeof(self) strongSelf) {
                    [strongSelf accountDetailsControllerWasDismissed];
                  },
                  weakSelf));
}

// Handles the account remove action from
// `self.removeOrMyGoogleChooserAlertCoordinator`. Action sheet created in
// `showAccountDetails:itemView:`
- (void)handleRemoveAccountWithIdentity:(id<SystemIdentity>)identity {
  DCHECK(self.removeOrMyGoogleChooserAlertCoordinator);
  // `self.removeOrMyGoogleChooserAlertCoordinator` should not be stopped, since
  // the coordinator has been confirmed.
  self.removeOrMyGoogleChooserAlertCoordinator = nil;
  DCHECK(!self.removeAccountCoordinator);
  NSString* title =
      l10n_util::GetNSStringF(IDS_IOS_REMOVE_ACCOUNT_ALERT_TITLE,
                              base::SysNSStringToUTF16(identity.userEmail));
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_REMOVE_ACCOUNT_CONFIRMATION_MESSAGE);
  self.removeAccountCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self
                                                   browser:_browser
                                                     title:title
                                                   message:message];
  __weak __typeof(self) weakSelf = self;
  [self.removeAccountCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "Signin_AccountsTableView_AccountDetail_RemoveAccount_"
                      "ConfirmationCancelled"));
                  weakSelf.removeAccountCoordinator = nil;
                  [weakSelf dismissRemoveAccountCoordinator];
                }
                 style:UIAlertActionStyleCancel];
  [self.removeAccountCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_REMOVE_ACCOUNT_LABEL)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "Signin_AccountsTableView_AccountDetail_RemoveAccount_"
                      "Confirmed"));
                  [weakSelf removeIdentity:identity];
                  [weakSelf dismissRemoveAccountCoordinator];
                }
                 style:UIAlertActionStyleDestructive];
  [self.removeAccountCoordinator start];
}

- (void)removeIdentity:(id<SystemIdentity>)identity {
  DCHECK(self.removeAccountCoordinator);
  self.removeAccountCoordinator = nil;
  self.uiDisabled = YES;
  __weak __typeof(self) weakSelf = self;
  GetApplicationContext()->GetSystemIdentityManager()->ForgetIdentity(
      identity, base::BindOnce(^(NSError* error) {
        weakSelf.uiDisabled = NO;
      }));
}

// Offer the user to sign-out near itemView
// If they sync, they can keep or delete their data.
- (void)showSignOutWithItemView:(UIView*)itemView {
  DCHECK(!self.signoutCoordinator);
  if (_authenticationOperationInProgress ||
      self != [self.navigationController topViewController]) {
    // An action is already in progress, ignore user's request.
    return;
  }

  constexpr signin_metrics::ProfileSignout metricSignOut =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;

  __weak LegacyAccountsTableViewController* weakSelf = self;
  self.signoutCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                            rect:itemView.frame
                            view:itemView
        forceSnackbarOverToolbar:NO
                      withSource:metricSignOut
                      completion:^(BOOL success, SceneState* scene_state) {
                        [weakSelf handleSignOutCompleted:success];
                      }];
  self.signoutCoordinator.delegate = self;
  [self.signoutCoordinator start];
}

// Handles the cancel action for `self.removeOrMyGoogleChooserAlertCoordinator`.
- (void)handleAlertCoordinatorCancel {
  DCHECK(self.removeOrMyGoogleChooserAlertCoordinator);
  // `self.removeOrMyGoogleChooserAlertCoordinator` should not be stopped, since
  // the coordinator has been cancelled.
  self.removeOrMyGoogleChooserAlertCoordinator = nil;
}

// Handles signout operation with `success` or failure.
- (void)handleSignOutCompleted:(BOOL)success {
  [self.signoutCoordinator stop];
  self.signoutCoordinator = nil;
  if (success) {
    [self handleAuthenticationOperationDidFinish];
  }
}

// Sets `_authenticationOperationInProgress` to NO and pops this accounts
// table view controller if the user is signed out.
- (void)handleAuthenticationOperationDidFinish {
  DCHECK(_authenticationOperationInProgress);
  _authenticationOperationInProgress = NO;
  [self popViewIfSignedOut];
}

- (void)popViewIfSignedOut {
  if (!_browser) {
    return;
  }

  if ([self authService]->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }
  if (_authenticationOperationInProgress) {
    // The signed out state might be temporary (e.g. account switch, ...).
    // Don't pop this view based on intermediary values.
    return;
  }
  if (_isBeingDismissed || _signoutDismissalByParentCoordinator) {
    return;
  }
  _isBeingDismissed = YES;
  __weak __typeof(self) weakSelf = self;
  void (^popAccountsTableViewController)() = ^() {
    [base::apple::ObjCCastStrict<SettingsNavigationController>(
        weakSelf.navigationController)
        popViewControllerOrCloseSettingsAnimated:YES];
  };
  if (!_accountDetailsControllerDismissCallback.is_null()) {
    DCHECK(self.presentedViewController);
    DCHECK(!self.removeOrMyGoogleChooserAlertCoordinator);
    DCHECK(!self.removeAccountCoordinator);
    DCHECK(!self.signoutCoordinator);
    // TODO(crbug.com/40056250): Need to add a completion block in
    // `dismissAccountDetailsViewControllerBlock` callback, to trigger
    // `popAccountsTableViewController()`.
    // Once we have a completion block, we can set `animated` to YES.
    std::move(_accountDetailsControllerDismissCallback).Run(/*animated=*/false);
    popAccountsTableViewController();
  } else if (self.removeOrMyGoogleChooserAlertCoordinator ||
             self.removeAccountCoordinator || self.signoutCoordinator) {
    DCHECK(self.presentedViewController);
    // If `self` is presenting a view controller (like
    // `self.removeOrMyGoogleChooserAlertCoordinator`,
    // `self.removeAccountCoordinator`, it has to be dismissed before `self` can
    // be poped from the navigation controller.
    // This issue can be easily reproduced with EG tests, but not with Chrome
    // app itself.
    [self
        dismissViewControllerAnimated:NO
                           completion:^{
                             [weakSelf.removeOrMyGoogleChooserAlertCoordinator
                                     stop];
                             weakSelf.removeOrMyGoogleChooserAlertCoordinator =
                                 nil;
                             [weakSelf.removeAccountCoordinator stop];
                             weakSelf.removeAccountCoordinator = nil;
                             [weakSelf.signoutCoordinator stop];
                             weakSelf.signoutCoordinator = nil;
                             popAccountsTableViewController();
                           }];
  } else {
    DCHECK(!self.presentedViewController);
    // Pops `self`.
    popAccountsTableViewController();
  }
}

#pragma mark - Access to authentication service

- (AuthenticationService*)authService {
  DCHECK(_browser) << "-authService called after -settingsWillBeDismissed";
  return AuthenticationServiceFactory::GetForProfile(_browser->GetProfile());
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSAccountsSettingsCloseWithSwipe"));
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  _authenticationOperationInProgress = YES;
  [self preventUserInteraction];
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self allowUserInteraction];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [_applicationHandler closePresentedViewsAndOpenURL:command];
}

#pragma mark - Private methods

- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

- (void)dismissRemoveOrMyGoogleChooserAlert {
  [self.removeOrMyGoogleChooserAlertCoordinator stop];
  self.removeOrMyGoogleChooserAlertCoordinator = nil;
}

- (void)dismissRemoveAccountCoordinator {
  [self.removeAccountCoordinator stop];
  self.removeAccountCoordinator = nil;
}

// Called with the account details controller has been dismissed.
- (void)accountDetailsControllerWasDismissed {
  _accountDetailsControllerDismissCallback.Reset();
}

#pragma mark - ManageAccountsConsumer

- (void)reloadAllItems {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self reloadData];
}

- (void)popView {
  [self popViewIfSignedOut];
}

- (void)updateIdentityViewItem:(IdentityViewItem*)identityViewItem {
  TableViewAccountItem* item =
      base::apple::ObjCCastStrict<TableViewAccountItem>(
          [_identityMap objectForKey:identityViewItem.gaiaID]);
  if (!item) {
    return;
  }
  [self updateAccountItem:item withIdentityViewItem:identityViewItem];
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
}

@end

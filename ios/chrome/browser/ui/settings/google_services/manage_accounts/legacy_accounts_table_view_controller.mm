// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/legacy_accounts_table_view_controller.h"

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
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
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
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_model_identity_data_source.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/identity_view_item.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
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
  SectionIdentifierError,
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
  // Indicates the errors related to the signed in account.
  ItemTypeAccountErrorMessage,
  // Button to resolve the account error.
  ItemTypeAccountErrorButton,
};

// Size of the symbols.
constexpr CGFloat kErrorSymbolSize = 22.;

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

  // The type of account error that is being displayed in the error section for
  // syncing accounts. Is set to kNone when there is no error section.
  syncer::SyncService::UserActionableError _diplayedAccountErrorType;

  // The type of actionable the syncing user needs to take to resolve the error.
  AccountErrorUserActionableType _accountErrorUserActionableType;

  // ApplicationCommands handler.
  id<ApplicationCommands> _applicationHandler;

  // If YES, AccountsTableViewController should not dismiss itself only for a
  // sign-out reason. The parent coordinator is responsible to dismiss this
  // coordinator when a sign-out happens.
  BOOL _signoutDismissalByParentCoordinator;
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
    _accountErrorUserActionableType = AccountErrorUserActionableType::kNoAction;
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
  _browser = nullptr;

  _isBeingDismissed = YES;
}

#pragma mark - SettingsRootTableViewController

- (void)reloadData {
  if (!_browser)
    return;

  if (![self authService]->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // This accounts table view will be popped or dismissed when the user
    // is signed out. Avoid reloading it in that case as that would lead to an
    // empty table view.
    return;
  }
  [super reloadData];
}

- (void)loadModel {
  if (!_browser)
    return;

  // Update the title with the name with the currently signed-in account.
  AuthenticationService* authService = self.authService;
  id<SystemIdentity> authenticatedIdentity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  self.title = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_ACCOUNTS_MANAGEMENT_FROM_ACCOUNT_SETTINGS_TITLE);

  [super loadModel];

  if (!authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin))
    return;

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

  // Account Storage errors section.
  [self updateErrorSectionModelAndReloadViewIfNeeded:NO];

  // Sign out section.
  [model addSectionWithIdentifier:SectionIdentifierSignOut];
  [model addItem:[self signOutItem]
      toSectionWithIdentifier:SectionIdentifierSignOut];

  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  BOOL hasSyncConsent =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSync);
  TableViewLinkHeaderFooterItem* footerItem = nil;
  if ([self authService]->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy) {
    if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
      footerItem =
          [self signOutSyncingFooterItemForForcedSignin:hasSyncConsent];
    }
  } else if (hasSyncConsent) {
    footerItem = [self signOutSyncingFooterItem];
  }

  [model setFooter:footerItem
      forSectionWithIdentifier:SectionIdentifierSignOut];
}

#pragma mark - Model objects

- (TableViewTextHeaderFooterItem*)signInHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeSignInHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_DESCRIPTION);
  return header;
}

- (TableViewLinkHeaderFooterItem*)signOutSyncingFooterItem {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutSyncingFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE);
  return footer;
}

- (TableViewLinkHeaderFooterItem*)signOutSyncingFooterItemForForcedSignin:
    (BOOL)syncConsent {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutSyncingFooter];

  if (syncConsent) {
    NSString* text = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE);
    text = [text stringByAppendingString:@"\n\n"];
    text = [text
        stringByAppendingString:
            l10n_util::GetNSString(
                IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE)];
    footer.text = text;
  } else {
    footer.text = l10n_util::GetNSString(
        IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE);
  }

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
  item.textColor = [self.modelIdentityDataSource isAccountSignedInNotSyncing]
                       ? [UIColor colorNamed:kBlueColor]
                       : [UIColor colorNamed:kRedColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSettingsAccountsTableViewSignoutCellId;
  return item;
}

// Initializes the passphrase error message item.
- (TableViewItem*)accountErrorMessageItemWithMessageID:(int)messageID {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:ItemTypeAccountErrorMessage];
  item.detailText = l10n_util::GetNSString(messageID);
  item.image =
      DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kErrorSymbolSize);
  item.imageViewTintColor = [UIColor colorNamed:kRed500Color];
  return item;
}

// Initializes the passphrase error button to open the passphrase dialog.
- (TableViewItem*)accountErrorButtonItemWithLabelID:(int)labelID {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeAccountErrorButton];
  item.text = l10n_util::GetNSString(labelID);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
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
    case ItemTypeAccountErrorButton: {
      base::RecordAction(
          base::UserMetricsAction("Signin_AccountsTableView_ErrorButton"));
      [self handleAccountErrorUserActionable];
      break;
    }
    case ItemTypeAccountErrorMessage:
      // Do not handle row selection on the account error message item because
      // its selection is disabled. The only purpose of the item is to show a
      // message that gives details on the error.
      break;
    case ItemTypeSignInHeader:
    case ItemTypeSignOutSyncingFooter:
    case ItemTypeRestrictedAccountsFooter:
      NOTREACHED_IN_MIGRATION();
      break;
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
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* completionInfo) {
                 [weakSelf handleDidAddAccount:result];
               }];
  [_applicationHandler showSignin:command baseViewController:self];
}

- (void)handleDidAddAccount:(SigninCoordinatorResult)result {
  // TODO(crbug.com/40229802): Remove the following line when todo bug will be
  // fixed.
  [self allowUserInteraction];
  [self handleAuthenticationOperationDidFinish];
  if (result == SigninCoordinatorResult::SigninCoordinatorResultSuccess &&
      _closeSettingsOnAddAccount) {
    [_applicationHandler closeSettingsUI];
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
  self.signoutCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                            rect:itemView.frame
                            view:itemView
        forceSnackbarOverToolbar:NO
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutSettings];
  __weak LegacyAccountsTableViewController* weakSelf = self;
  self.signoutCoordinator.signoutCompletion = ^(BOOL success) {
    [weakSelf.signoutCoordinator stop];
    weakSelf.signoutCoordinator = nil;
    if (success) {
      [weakSelf handleAuthenticationOperationDidFinish];
    }
  };
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

// Sets `_authenticationOperationInProgress` to NO and pops this accounts
// table view controller if the user is signed out.
- (void)handleAuthenticationOperationDidFinish {
  DCHECK(_authenticationOperationInProgress);
  _authenticationOperationInProgress = NO;
  [self popViewIfSignedOut];
}

- (void)popViewIfSignedOut {
  if (!_browser)
    return;

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
  [_applicationHandler closeSettingsUIAndOpenURL:command];
}

#pragma mark - Internal

- (void)handleAccountErrorUserActionable {
  switch (_accountErrorUserActionableType) {
    case AccountErrorUserActionableType::kEnterPassphrase: {
      [self openPassphraseDialog];
      break;
    }
    case AccountErrorUserActionableType::kReauthForFetchKeys: {
      [self openTrustedVaultReauthForFetchKeys];
      break;
    }
    case AccountErrorUserActionableType::kReauthForDegradedRecoverability: {
      [self openTrustedVaultReauthForDegradedRecoverability];
      break;
    }
    case AccountErrorUserActionableType::kNoAction:
      break;
  }
}

// Opens the trusted vault reauth dialog for fetch keys.
- (void)openTrustedVaultReauthForFetchKeys {
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
  [_applicationHandler
      showTrustedVaultReauthForFetchKeysFromViewController:self
                                          securityDomainID:securityDomainID
                                                   trigger:trigger
                                               accessPoint:accessPoint];
}

// Opens the trusted vault reauth dialog for degraded recoverability.
- (void)openTrustedVaultReauthForDegradedRecoverability {
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
  [_applicationHandler
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:self
                                                       securityDomainID:
                                                           securityDomainID
                                                                trigger:trigger
                                                            accessPoint:
                                                                accessPoint];
}

// Opens the passphrase dialog.
- (void)openPassphraseDialog {
  UIViewController<SettingsRootViewControlling>* controllerToPush =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:_browser];

  // Verify that the accounts table is displayed from a navigation controller.
  DCHECK(self.navigationController);

  [self configureHandlersForRootViewController:controllerToPush];
  [self.navigationController pushViewController:controllerToPush animated:YES];
}

#pragma mark - Private methods

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

#pragma mark - AccountsConsumer

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

// Updates the error section in the table view model to indicate the latest
// account error if the states of the account error and the table view model
// don't match. If `reloadViewIfNeeded` is NO, only the model will be
// updated without reloading the view. Can refresh, add or remove the error
// section when an update is needed.
- (void)updateErrorSectionModelAndReloadViewIfNeeded:(BOOL)reloadViewIfNeeded {
  if ([self.modelIdentityDataSource isAccountSignedInNotSyncing]) {
    // If the account is signed in not syncing, the error handling will be
    // shown previously in account settings page and no need to load it in
    // this view.
    return;
  }
  AccountErrorUIInfo* errorInfo =
      [self.modelIdentityDataSource accountErrorUIInfo];
  BOOL hadErrorSection = [self.tableViewModel
      hasSectionForSectionIdentifier:SectionIdentifierError];
  syncer::SyncService::UserActionableError newErrorType =
      errorInfo ? errorInfo.errorType
                : syncer::SyncService::UserActionableError::kNone;

  if (newErrorType == syncer::SyncService::UserActionableError::kNone &&
      _diplayedAccountErrorType ==
          syncer::SyncService::UserActionableError::kNone) {
    DCHECK(!hadErrorSection);
    // Don't update if there is no error to indicate or to remove.
    return;
  }

  if (reloadViewIfNeeded && newErrorType == _diplayedAccountErrorType) {
    DCHECK(hadErrorSection);
    // Don't update if there is already a model and a view, and the state of
    // the model already matches the error that has to be indicated.
    return;
  }

  _diplayedAccountErrorType = newErrorType;
  _accountErrorUserActionableType = errorInfo.userActionableType;

  if (hadErrorSection) {
    // Remove the section from the model to either clear the error section
    // when there is no error or to update the type of error to indicate.
    NSUInteger index = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierError];
    [self.tableViewModel removeSectionWithIdentifier:SectionIdentifierError];

    if (errorInfo == nil) {
      // Delete the error section in the view when there is an error section
      // while there is no account error to indicate.
      if (reloadViewIfNeeded) {
        [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:index]
                      withRowAnimation:UITableViewRowAnimationAutomatic];
      }
      return;
    }
  }

  // Update the error section in the model to indicate the latest account
  // error.
  NSInteger sectionIndex =
      [self.tableViewModel
          sectionForSectionIdentifier:SectionIdentifierAccounts] +
      1;
  [self.tableViewModel insertSectionWithIdentifier:SectionIdentifierError
                                           atIndex:sectionIndex];
  [self.tableViewModel addItem:[self accountErrorMessageItemWithMessageID:
                                         errorInfo.messageID]
       toSectionWithIdentifier:SectionIdentifierError];
  [self.tableViewModel addItem:[self accountErrorButtonItemWithLabelID:
                                         errorInfo.buttonLabelID]
       toSectionWithIdentifier:SectionIdentifierError];

  if (reloadViewIfNeeded) {
    if (hadErrorSection) {
      // Only refresh the section if there was already an error section, where
      // there was a change in the type of error to indicate (excluding
      // kNone).
      [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
    } else {
      [self.tableView insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
    }
  }
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

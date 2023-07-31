// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/settings/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;
using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

namespace {

// The size of the symbol image.
const CGFloat kSymbolAddAccountPointSize = 20;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAccounts = kSectionIdentifierEnumZero,
  SectionIdentifierError,
  SectionIdentifierSignOut,
};

typedef NS_ENUM(NSInteger, ItemType) {
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

@interface AccountsTableViewController () <
    ChromeAccountManagerServiceObserver,
    IdentityManagerObserverBridgeDelegate,
    SignoutActionSheetCoordinatorDelegate,
    SyncObserverModelBridge> {
  Browser* _browser;
  BOOL _closeSettingsOnAddAccount;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Whether an authentication operation is in progress (e.g switch accounts,
  // sign out).
  BOOL _authenticationOperationInProgress;
  // Whether the view controller is currently being dismissed and new dismiss
  // requests should be ignored.
  BOOL _isBeingDismissed;

  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, TableViewItem*>* _identityMap;

  std::unique_ptr<SyncObserverBridge> _syncObserver;

  // The type of account error that is being displayed in the error section.
  // Is set to kNone when there is no error section.
  syncer::SyncService::UserActionableError _diplayedAccountErrorType;

  // The type of actionable the user needs to take to resolve the error.
  AccountErrorUserActionableType _accountErrorUserActionableType;
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

// AccountManager Service used to retrive identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

// Stops observing browser state services. This is required during the shutdown
// phase to avoid observing services for a browser state that is being killed.
- (void)stopBrowserStateServiceObservers;

@end

@implementation AccountsTableViewController {
  // Callback to dismiss MyGoogle (Account Detail).
  DismissViewCallback _dismissAccountDetailsViewController;
}

- (instancetype)initWithBrowser:(Browser*)browser
      closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _accountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            _browser->GetBrowserState());
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            IdentityManagerFactory::GetForBrowserState(
                _browser->GetBrowserState()),
            self);
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(_browser->GetBrowserState());
    DCHECK(syncService);
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
    _diplayedAccountErrorType = syncer::SyncService::UserActionableError::kNone;
    _accountErrorUserActionableType = AccountErrorUserActionableType::kNoAction;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kSettingsAccountsTableViewId;

  [self loadModel];
}

- (void)stopBrowserStateServiceObservers {
  _identityManagerObserver.reset();
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAccountsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAccountsSettingsBack"));
}

- (void)settingsWillBeDismissed {
  [self.removeOrMyGoogleChooserAlertCoordinator stop];
  self.removeOrMyGoogleChooserAlertCoordinator = nil;
  [self.signoutCoordinator stop];
  self.signoutCoordinator = nil;
  [self.removeAccountCoordinator stop];
  self.removeAccountCoordinator = nil;
  [self stopBrowserStateServiceObservers];
  _accountManagerServiceObserver.reset();
  _syncObserver.reset();
  _browser = nullptr;
  _accountManagerService = nullptr;

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

  NSString* title = nil;
  if (authenticatedIdentity) {
    title = authenticatedIdentity.userFullName;
    if (!title) {
      title = authenticatedIdentity.userEmail;
    }
  }
  if ([self isAccountSignedInNotSyncing]) {
    title = l10n_util::GetNSString(
        IDS_IOS_GOOGLE_ACCOUNTS_MANAGEMENT_FROM_ACCOUNT_SETTINGS_TITLE);
  }
  self.title = title;

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
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browser->GetBrowserState());

  NSString* authenticatedEmail = authenticatedIdentity.userEmail;
  for (const auto& account : identityManager->GetAccountsWithRefreshTokens()) {
    id<SystemIdentity> identity =
        self.accountManagerService->GetIdentityWithGaiaID(account.gaia);
    if (!identity) {
      // Ignore the case in which the identity is invalid at lookup time. This
      // may be due to inconsistencies between the identity service and
      // ProfileOAuth2TokenService.
      continue;
    }
    // TODO(crbug.com/1081274): This re-ordering will be redundant once we
    // apply ordering changes to the account reconciler.
    TableViewItem* item = [self accountItem:identity];
    if ([identity.userEmail isEqualToString:authenticatedEmail]) {
      [model insertItem:item
          inSectionWithIdentifier:SectionIdentifierAccounts
                          atIndex:0];
    } else {
      [model addItem:item toSectionWithIdentifier:SectionIdentifierAccounts];
    }

    [mutableIdentityMap setObject:item forKey:identity.gaiaID];
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

- (TableViewItem*)accountItem:(id<SystemIdentity>)identity {
  TableViewAccountItem* item =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  [self updateAccountItem:item withIdentity:identity];
  return item;
}

- (void)updateAccountItem:(TableViewAccountItem*)item
             withIdentity:(id<SystemIdentity>)identity {
  item.image = self.accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::TableViewIcon);
  item.text = identity.userEmail;
  item.identity = identity;
  item.accessibilityIdentifier = identity.userEmail;
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
  item.textColor = [self isAccountSignedInNotSyncing]
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

// Updates the error section in the table view model to indicate the latest
// account error if the states of the account error and the table view model
// don't match. If `reloadViewIfNeeded` is NO, only the model will be
// updated without reloading the view. Can refresh, add or remove the error
// section when an update is needed.
- (void)updateErrorSectionModelAndReloadViewIfNeeded:(BOOL)reloadViewIfNeeded {
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(_browser->GetBrowserState());
  DCHECK(syncService);
  AccountErrorUIInfo* errorInfo = GetAccountErrorUIInfo(syncService);
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
    // Remove the section from the model to either clear the error section when
    // there is no error or to update the type of error to indicate.
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

  // Update the error section in the model to indicate the latest account error.
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
      // there was a change in the type of error to indicate (excluding kNone).
      [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
    } else {
      [self.tableView insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
    }
  }
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
          base::mac::ObjCCast<TableViewLinkHeaderFooterView>(view);
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

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  switch (itemType) {
    case ItemTypeAccount: {
      TableViewAccountItem* item =
          base::mac::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      DCHECK(item.identity);

      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showAccountDetails:item.identity itemView:itemView];
      break;
    }
    case ItemTypeAddAccount: {
      [self showAddAccount];
      break;
    }
    case ItemTypeSignOut: {
      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showSignOutWithItemView:itemView];
      break;
    }
    case ItemTypeAccountErrorButton: {
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
      NOTREACHED();
      break;
  }

  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateErrorSectionModelAndReloadViewIfNeeded:YES];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  DCHECK(_browser) << "-onEndBatchOfRefreshTokenStateChanges called after "
                      "-stopBrowserStateServiceObservers";

  [self reloadData];
  // Only attempt to pop the top-most view controller once the account list
  // has been dismissed.
  [self popViewIfSignedOut];
}

#pragma mark - Authentication operations

- (void)showAddAccount {
  DCHECK(!self.removeOrMyGoogleChooserAlertCoordinator);
  _authenticationOperationInProgress = YES;

  // TODO(crbug.com/1338990): Remove the following line when todo bug will be
  // fixed.
  [self preventUserInteraction];
  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperationAddAccount
               identity:nil
            accessPoint:AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result) {
                 BOOL success = result == SigninCoordinatorResultSuccess;
                 [weakSelf handleDidAddAccount:success];
               }];
  [self.applicationCommandsHandler showSignin:command baseViewController:self];
}

- (void)handleDidAddAccount:(BOOL)success {
  // TODO(crbug.com/1338990): Remove the following line when todo bug will be
  // fixed.
  [self allowUserInteraction];
  [self handleAuthenticationOperationDidFinish];
  if (success && _closeSettingsOnAddAccount) {
    [self.applicationCommandsHandler closeSettingsUI];
  }
}

- (void)showAccountDetails:(id<SystemIdentity>)identity
                  itemView:(UIView*)itemView {
  DCHECK(!self.removeOrMyGoogleChooserAlertCoordinator);
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
                  [weakSelf handleManageGoogleAccountWithIdentity:identity];
                }
                 style:UIAlertActionStyleDefault];
  [self.removeOrMyGoogleChooserAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_REMOVE_GOOGLE_ACCOUNT_TITLE)
                action:^{
                  [weakSelf handleRemoveSecondaryAccountWithIdentity:identity];
                }
                 style:UIAlertActionStyleDestructive];
  [self.removeOrMyGoogleChooserAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^() {
                  [weakSelf handleAlertCoordinatorCancel];
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
  _dismissAccountDetailsViewController =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentAccountDetailsController(identity, self,
                                            /*animated=*/YES);
}

// Handles the secondary account remove action from
// `self.removeOrMyGoogleChooserAlertCoordinator`. Action sheet created in
// `showAccountDetails:itemView:`
- (void)handleRemoveSecondaryAccountWithIdentity:(id<SystemIdentity>)identity {
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
                  weakSelf.removeAccountCoordinator = nil;
                }
                 style:UIAlertActionStyleCancel];
  [self.removeAccountCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_REMOVE_ACCOUNT_LABEL)
                action:^{
                  [weakSelf removeSecondaryIdentity:identity];
                }
                 style:UIAlertActionStyleDestructive];
  [self.removeAccountCoordinator start];
}

- (void)removeSecondaryIdentity:(id<SystemIdentity>)identity {
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
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutSettings];
  __weak AccountsTableViewController* weakSelf = self;
  self.signoutCoordinator.completion = ^(BOOL success) {
    [weakSelf.signoutCoordinator stop];
    weakSelf.signoutCoordinator = nil;
    if (success) {
      [weakSelf handleAuthenticationOperationDidFinish];
    }
  };
  self.signoutCoordinator.delegate = self;
  [self.signoutCoordinator start];
}

// Logs the UMA metrics to record the data retention option selected by the user
// on signout. If the account is managed the data will always be cleared.
- (void)logSignoutMetricsWithForceClearData:(BOOL)forceClearData {
  if (![self authService]->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin)) {
    UMA_HISTOGRAM_BOOLEAN("Signin.UserRequestedWipeDataOnSignout",
                          forceClearData);
  }
  if (forceClearData) {
    base::RecordAction(base::UserMetricsAction("Signin_SignoutClearData"));
  } else {
    base::RecordAction(base::UserMetricsAction("Signin_Signout"));
  }
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
  if (_isBeingDismissed) {
    return;
  }
  _isBeingDismissed = YES;
  __weak __typeof(self) weakSelf = self;
  void (^popAccountsTableViewController)() = ^() {
    [base::mac::ObjCCastStrict<SettingsNavigationController>(
        weakSelf.navigationController)
        popViewControllerOrCloseSettingsAnimated:YES];
  };
  if (!_dismissAccountDetailsViewController.is_null()) {
    DCHECK(self.presentedViewController);
    DCHECK(!self.removeOrMyGoogleChooserAlertCoordinator);
    DCHECK(!self.removeAccountCoordinator);
    DCHECK(!self.signoutCoordinator);
    // TODO(crbug.com/1221066): Need to add a completion block in
    // `dismissAccountDetailsViewControllerBlock` callback, to trigger
    // `popAccountsTableViewController()`.
    // Once we have a completion block, we can set `animated` to YES.
    std::move(_dismissAccountDetailsViewController).Run(/*animated*/ false);
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
  return AuthenticationServiceFactory::GetForBrowserState(
      _browser->GetBrowserState());
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  TableViewAccountItem* item = base::mac::ObjCCastStrict<TableViewAccountItem>(
      [_identityMap objectForKey:identity.gaiaID]);
  if (!item) {
    return;
  }
  [self updateAccountItem:item withIdentity:identity];
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
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
  [self.applicationCommandsHandler closeSettingsUIAndOpenURL:command];
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
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  [self.applicationCommandsHandler
      showTrustedVaultReauthForFetchKeysFromViewController:self
                                                   trigger:trigger];
}

// Opens the trusted vault reauth dialog for degraded recoverability.
- (void)openTrustedVaultReauthForDegradedRecoverability {
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  [self.applicationCommandsHandler
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:self
                                                                trigger:
                                                                    trigger];
}

// Opens the passphrase dialog.
- (void)openPassphraseDialog {
  UIViewController<SettingsRootViewControlling>* controllerToPush =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:_browser];

  // Verify that the accounts table is displayed from a navigation controller.
  DCHECK(self.navigationController);

  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  controllerToPush.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      _browser->GetCommandDispatcher());
  [self.navigationController pushViewController:controllerToPush animated:YES];
}

#pragma mark - Private methods

// Returns YES if the account is signed in not syncing, NO otherwise.
- (BOOL)isAccountSignedInNotSyncing {
  return base::FeatureList::IsEnabled(
             syncer::kReplaceSyncPromosWithSignInPromos) &&
         !SyncServiceFactory::GetForBrowserState(_browser->GetBrowserState())
              ->HasSyncConsent();
}

@end

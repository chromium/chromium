// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"

#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_browser_opener.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAccounts = kSectionIdentifierEnumZero,
  SectionIdentifierSync,
  SectionIdentifierSignOut,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAccount = kItemTypeEnumZero,
  ItemTypeAddAccount,
  // Provides sign out items used only for non-managed accounts.
  ItemTypeSignOut,
  // Sign out item that clears Chrome data. Used for both managed
  // and non-managed accounts.
  ItemTypeSignOutAndClearData,
  ItemTypeHeader,
  // Detailed description of the actions taken by sign out e.g. turning off sync
  // and clearing Chrome data.
  ItemTypeSignOutManagedAccountFooter,
  // Detailed description of the actions taken by sign out, e.g. turning off
  // sync.
  ItemTypeSignOutNonManagedAccountFooter,
  // Detailed description of the actions taken by sign out, e.g. turning off
  // sync. Related to kSimplifySignOutIOS feature only.
  ItemTypeSignOutSyncingFooter,
};

}  // namespace

@interface AccountsTableViewController () <
    ChromeIdentityServiceObserver,
    ChromeIdentityBrowserOpener,
    IdentityManagerObserverBridgeDelegate,
    SignoutActionSheetCoordinatorDelegate> {
  Browser* _browser;
  BOOL _closeSettingsOnAddAccount;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Whether an authentication operation is in progress (e.g switch accounts,
  // sign out).
  BOOL _authenticationOperationInProgress;
  // Whether the view controller is currently being dismissed and new dismiss
  // requests should be ignored.
  BOOL _isBeingDismissed;
  ResizedAvatarCache* _avatarCache;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;

  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, TableViewItem*>* _identityMap;
}

// Modal alert for sign out.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Callback to dismiss MyGoogle (Account Detail).
@property(nonatomic, copy)
    ios::DismissASMViewControllerBlock dismissAccountDetailsViewControllerBlock;

// Modal alert for confirming account removal.
@property(nonatomic, strong) AlertCoordinator* removeAccountCoordinator;

// Modal alert for sign out in experiment kSimplifySignOutIOS.
@property(nonatomic, strong) SignoutActionSheetCoordinator* signoutCoordinator;

// If YES, the UI elements are disabled.
@property(nonatomic, assign) BOOL uiDisabled;

// Stops observing browser state services. This is required during the shutdown
// phase to avoid observing services for a browser state that is being killed.
- (void)stopBrowserStateServiceObservers;

@end

@implementation AccountsTableViewController

@synthesize dispatcher = _dispatcher;

- (instancetype)initWithBrowser:(Browser*)browser
      closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            IdentityManagerFactory::GetForBrowserState(
                _browser->GetBrowserState()),
            self);
    _avatarCache = [[ResizedAvatarCache alloc] init];
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));
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
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
  [self.signoutCoordinator stop];
  self.signoutCoordinator = nil;
  [self.removeAccountCoordinator stop];
  self.removeAccountCoordinator = nil;
  [self stopBrowserStateServiceObservers];
  _browser = nullptr;
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
  ChromeIdentity* authenticatedIdentity =
      [self authService]->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  NSString* title = nil;
  if (authenticatedIdentity) {
    title = [authenticatedIdentity userFullName];
    if (!title) {
      title = [authenticatedIdentity userEmail];
    }
  }
  self.title = title;

  [super loadModel];

  if (![self authService]->HasPrimaryIdentity(signin::ConsentLevel::kSignin))
    return;

  TableViewModel* model = self.tableViewModel;

  NSMutableDictionary<NSString*, TableViewItem*>* mutableIdentityMap =
      [[NSMutableDictionary alloc] init];

  // Account cells.
  [model addSectionWithIdentifier:SectionIdentifierAccounts];
  [model setHeader:[self header]
      forSectionWithIdentifier:SectionIdentifierAccounts];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browser->GetBrowserState());

  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          _browser->GetBrowserState());

  NSString* authenticatedEmail = [authenticatedIdentity userEmail];
  for (const auto& account : identityManager->GetAccountsWithRefreshTokens()) {
    ChromeIdentity* identity =
        accountManagerService->GetIdentityWithGaiaID(account.gaia);
    if (!identity) {
      // Ignore the case in which the identity is invalid at lookup time. This
      // may be due to inconsistencies between the identity service and
      // ProfileOAuth2TokenService.
      continue;
    }
    // TODO(crbug.com/1081274): This re-ordering will be redundant once we
    // apply ordering changes to the account reconciler.
    TableViewItem* item = [self accountItem:identity];
    if ([identity.userEmail isEqual:authenticatedEmail]) {
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

  // Sign out section.
  [model addSectionWithIdentifier:SectionIdentifierSignOut];
  if (base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS)) {
    [model addItem:[self signOutItem]
        toSectionWithIdentifier:SectionIdentifierSignOut];
  } else {
    // Adds a signout option if the account is not managed.
    if (![self authService]->HasPrimaryIdentityManaged(
            signin::ConsentLevel::kSignin)) {
      [model addItem:[self signOutItem]
          toSectionWithIdentifier:SectionIdentifierSignOut];
    }
    // Adds a signout and clear data option.
    [model addItem:[self signOutAndClearDataItem]
        toSectionWithIdentifier:SectionIdentifierSignOut];
  }

  // Adds a footer with signout explanation depending on the type of
  // account whether managed or non-managed.
  if (base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS)) {
    SyncSetupService* syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(
            _browser->GetBrowserState());
    if (syncSetupService->IsFirstSetupComplete()) {
      [model setFooter:[self signOutSyncingFooterItem]
          forSectionWithIdentifier:SectionIdentifierSignOut];
    }
  } else if ([self authService]->HasPrimaryIdentityManaged(
                 signin::ConsentLevel::kSignin)) {
    [model setFooter:[self signOutManagedAccountFooterItem]
        forSectionWithIdentifier:SectionIdentifierSignOut];
  } else {
    [model setFooter:[self signOutNonManagedAccountFooterItem]
        forSectionWithIdentifier:SectionIdentifierSignOut];
  }
}

#pragma mark - Model objects

- (TableViewTextHeaderFooterItem*)header {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_DESCRIPTION);
  return header;
}

- (TableViewLinkHeaderFooterItem*)signOutNonManagedAccountFooterItem {
  DCHECK(!base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS));
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutNonManagedAccountFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_DISCONNECT_NON_MANAGED_ACCOUNT_FOOTER_INFO_MOBILE);
  return footer;
}

- (TableViewLinkHeaderFooterItem*)signOutManagedAccountFooterItem {
  DCHECK(!base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS));
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutManagedAccountFooter];
  std::u16string hostedDomain = HostedDomainForPrimaryAccount(_browser);
  footer.text = l10n_util::GetNSStringF(
      IDS_IOS_DISCONNECT_MANAGED_ACCOUNT_FOOTER_INFO_MOBILE, hostedDomain);
  return footer;
}

- (TableViewLinkHeaderFooterItem*)signOutSyncingFooterItem {
  DCHECK(base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS));
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutSyncingFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE);
  return footer;
}

- (TableViewItem*)accountItem:(ChromeIdentity*)identity {
  TableViewAccountItem* item =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  [self updateAccountItem:item withIdentity:identity];
  return item;
}

- (void)updateAccountItem:(TableViewAccountItem*)item
             withIdentity:(ChromeIdentity*)identity {
  item.image = [_avatarCache resizedAvatarForIdentity:identity];
  item.text = identity.userEmail;
  item.chromeIdentity = identity;
  item.accessibilityIdentifier = identity.userEmail;
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
}

- (TableViewItem*)addAccountItem {
  TableViewAccountItem* item =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAddAccount];
  item.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_ADD_ACCOUNT_BUTTON);
  item.accessibilityIdentifier = kSettingsAccountsTableViewAddAccountCellId;
  item.image = [[UIImage imageNamed:@"settings_accounts_add_account"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  return item;
}

- (TableViewItem*)signOutItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeSignOut];
  item.text =
      l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  if (base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS)) {
    item.textColor = [UIColor colorNamed:kRedColor];
  } else {
    item.textColor = [UIColor colorNamed:kBlueColor];
  }
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSettingsAccountsTableViewSignoutCellId;
  return item;
}

- (TableViewItem*)signOutAndClearDataItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeSignOutAndClearData];
  item.text = l10n_util::GetNSString(
      IDS_IOS_DISCONNECT_DIALOG_CONTINUE_AND_CLEAR_MOBILE);
  item.textColor = [UIColor colorNamed:kRedColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier =
      kSettingsAccountsTableViewSignoutAndClearDataCellId;
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // If there is an operation in process that does not allow selecting a cell
  // exit without performing the selection.
  if (self.uiDisabled) {
    return;
  }

  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeAccount: {
      TableViewAccountItem* item =
          base::mac::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      DCHECK(item.chromeIdentity);

      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showAccountDetails:item.chromeIdentity itemView:itemView];
      break;
    }
    case ItemTypeAddAccount: {
      [self showAddAccount];
      break;
    }
    case ItemTypeSignOut: {
      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      if (base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS)) {
        [self showMICESignOutWithItemView:itemView];
      } else {
        [self showSignOutWithClearData:NO itemView:itemView];
      }
      break;
    }
    case ItemTypeSignOutAndClearData: {
      DCHECK(!base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS));
      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showSignOutWithClearData:YES itemView:itemView];
      break;
    }
    default:
      break;
  }

  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
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
  DCHECK(!self.alertCoordinator);
  _authenticationOperationInProgress = YES;

  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_ADD_ACCOUNT
               identity:nil
            accessPoint:AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(BOOL success) {
                 [weakSelf handleDidAddAccount:success];
               }];
  DCHECK(self.dispatcher);
  [self.dispatcher showSignin:command baseViewController:self];
}

- (void)handleDidAddAccount:(BOOL)success {
  [self handleAuthenticationOperationDidFinish];
  if (success && _closeSettingsOnAddAccount) {
    [self.dispatcher closeSettingsUI];
  }
}

- (void)showAccountDetails:(ChromeIdentity*)identity
                  itemView:(UIView*)itemView {
  DCHECK(!self.alertCoordinator);
  self.alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:nil
                         message:identity.userEmail
                            rect:itemView.frame
                            view:itemView];
  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE)
                action:^{
                  [weakSelf handleManageGoogleAccountWithIdentity:identity];
                }
                 style:UIAlertActionStyleDefault];
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_REMOVE_GOOGLE_ACCOUNT_TITLE)
                action:^{
                  [weakSelf handleRemoveSecondaryAccountWithIdentity:identity];
                }
                 style:UIAlertActionStyleDestructive];
  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                   action:^() {
                                     [weakSelf handleAlertCoordinatorCancel];
                                   }
                                    style:UIAlertActionStyleCancel];
  [self.alertCoordinator start];
}

// Handles the manage Google account action from |self.alertCoordinator|.
// Action sheet created in |showAccountDetails:itemView:|
- (void)handleManageGoogleAccountWithIdentity:(ChromeIdentity*)identity {
  DCHECK(self.alertCoordinator);
  // |self.alertCoordinator| should not be stopped, since the coordinator has
  // been confirmed.
  self.alertCoordinator = nil;
  self.dismissAccountDetailsViewControllerBlock =
      ios::GetChromeBrowserProvider()
          .GetChromeIdentityService()
          ->PresentAccountDetailsController(identity, self,
                                            /*animated=*/YES);
}

// Handles the secondary account remove action from |self.alertCoordinator|.
// Action sheet created in |showAccountDetails:itemView:|
- (void)handleRemoveSecondaryAccountWithIdentity:(ChromeIdentity*)identity {
  DCHECK(self.alertCoordinator);
  // |self.alertCoordinator| should not be stopped, since the coordinator has
  // been confirmed.
  self.alertCoordinator = nil;
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

- (void)removeSecondaryIdentity:(ChromeIdentity*)identity {
  DCHECK(self.removeAccountCoordinator);
  self.removeAccountCoordinator = nil;
  self.uiDisabled = YES;
  ios::GetChromeBrowserProvider().GetChromeIdentityService()->ForgetIdentity(
      identity, ^(NSError* error) {
        self.uiDisabled = NO;
      });
}

- (void)showMICESignOutWithItemView:(UIView*)itemView {
  DCHECK(!self.signoutCoordinator);
  DCHECK(base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS));
  if (_authenticationOperationInProgress ||
      self != [self.navigationController topViewController]) {
    // An action is already in progress, ignore user's request.
    return;
  }
  self.signoutCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                            rect:itemView.frame
                            view:itemView];
  __weak AccountsTableViewController* weakSelf = self;
  self.signoutCoordinator.completion = ^(BOOL success) {
    if (success) {
      // Allow user interaction only didn't cancel the dialog.
      // if -[<SignoutActionSheetCoordinatorDelegate>
      // didSelectSignoutDataRetentionStrategy] has been called.
      [weakSelf allowUserInteraction];
    }
    [weakSelf.signoutCoordinator stop];
    weakSelf.signoutCoordinator = nil;
    if (success) {
      [weakSelf handleAuthenticationOperationDidFinish];
    }
  };
  self.signoutCoordinator.delegate = self;
  [self.signoutCoordinator start];
}

- (void)showSignOutWithClearData:(BOOL)forceClearData
                        itemView:(UIView*)itemView {
  DCHECK(!self.alertCoordinator);
  DCHECK(!base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS));
  if (_authenticationOperationInProgress ||
      self != [self.navigationController topViewController]) {
    // An action is already in progress, ignore user's request.
    return;
  }

  NSString* alertMessage = nil;
  NSString* signOutTitle = nil;
  UIAlertActionStyle actionStyle = UIAlertActionStyleDefault;

  if (forceClearData) {
    alertMessage = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DESTRUCTIVE_DIALOG_INFO_MOBILE);
    signOutTitle = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DIALOG_CONTINUE_AND_CLEAR_MOBILE);
    actionStyle = UIAlertActionStyleDestructive;
  } else {
    alertMessage =
        l10n_util::GetNSString(IDS_IOS_DISCONNECT_KEEP_DATA_DIALOG_INFO_MOBILE);
    signOutTitle = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
    actionStyle = UIAlertActionStyleDefault;
  }

  self.alertCoordinator =
      [[ActionSheetCoordinator alloc] initWithBaseViewController:self
                                                         browser:_browser
                                                           title:nil
                                                         message:alertMessage
                                                            rect:itemView.frame
                                                            view:itemView];

  __weak AccountsTableViewController* weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:signOutTitle
                action:^{
                  [weakSelf handleSignOutWithForceClearData:forceClearData];
                }
                 style:actionStyle];
  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                   action:^() {
                                     [weakSelf handleAlertCoordinatorCancel];
                                   }
                                    style:UIAlertActionStyleCancel];
  [self.alertCoordinator start];
}

- (void)handleSignOutWithForceClearData:(BOOL)forceClearData {
  if (!_browser)
    return;

  // |self.alertCoordinator| should not be stopped, since the coordinator has
  // been confirmed.
  DCHECK(self.alertCoordinator);
  self.alertCoordinator = nil;

  AuthenticationService* authService = [self authService];
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    _authenticationOperationInProgress = YES;
    [self preventUserInteraction];
    __weak AccountsTableViewController* weakSelf = self;
    authService->SignOut(
        signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, forceClearData, ^{
          // Metrics logging must occur before dismissing the currently
          // presented view controller from |handleSignoutDidFinish|.
          [weakSelf logSignoutMetricsWithForceClearData:forceClearData];
          [weakSelf allowUserInteraction];
          [weakSelf handleAuthenticationOperationDidFinish];
        });
  }
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
    base::RecordAction(base::UserMetricsAction(
        "Signin_SignoutClearData_FromAccountListSettings"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Signin_Signout_FromAccountListSettings"));
  }
}

// Handles the cancel action for |self.alertCoordinator|.
- (void)handleAlertCoordinatorCancel {
  DCHECK(self.alertCoordinator);
  // |self.alertCoordinator| should not be stopped, since the coordinator has
  // been cancelled.
  self.alertCoordinator = nil;
}

// Sets |_authenticationOperationInProgress| to NO and pops this accounts
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
  if (self.dismissAccountDetailsViewControllerBlock) {
    DCHECK(self.presentedViewController);
    DCHECK(!self.alertCoordinator);
    DCHECK(!self.removeAccountCoordinator);
    DCHECK(!self.signoutCoordinator);
    // TODO(crbug.com/1221066): Need to add a completion block in
    // |dismissAccountDetailsViewControllerBlock| callback, to trigger
    // |popAccountsTableViewController()|.
    // Once we have a completion block, we can set |animated| to YES.
    self.dismissAccountDetailsViewControllerBlock(/*animated=*/NO);
    self.dismissAccountDetailsViewControllerBlock = nil;
    popAccountsTableViewController();
  } else if (self.alertCoordinator || self.removeAccountCoordinator ||
             self.signoutCoordinator) {
    DCHECK(self.presentedViewController);
    // If |self| is presenting a view controller (like |self.alertCoordinator|,
    // |self.removeAccountCoordinator|, it has to be dismissed before |self| can
    // be poped from the navigation controller.
    // This issue can be easily reproduced with EG tests, but not with Chrome
    // app itself.
    [self dismissViewControllerAnimated:NO
                             completion:^{
                               [weakSelf.alertCoordinator stop];
                               weakSelf.alertCoordinator = nil;
                               [weakSelf.removeAccountCoordinator stop];
                               weakSelf.removeAccountCoordinator = nil;
                               [weakSelf.signoutCoordinator stop];
                               weakSelf.signoutCoordinator = nil;
                               popAccountsTableViewController();
                             }];
  } else {
    DCHECK(!self.presentedViewController);
    // Pops |self|.
    popAccountsTableViewController();
  }
}

#pragma mark - Access to authentication service

- (AuthenticationService*)authService {
  DCHECK(_browser) << "-authService called after -settingsWillBeDismissed";
  return AuthenticationServiceFactory::GetForBrowserState(
      _browser->GetBrowserState());
}

#pragma mark - ChromeIdentityBrowserOpener

- (void)openURL:(NSURL*)url
              view:(UIView*)view
    viewController:(UIViewController*)viewController {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:net::GURLWithNSURL(url)];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - ChromeIdentityServiceObserver

- (void)profileUpdate:(ChromeIdentity*)identity {
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

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSAccountsSettingsCloseWithSwipe"));
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)didSelectSignoutDataRetentionStrategy {
  _authenticationOperationInProgress = YES;
  [self preventUserInteraction];
}

@end

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"

#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_browser_opener.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSettingsAccountsTableViewId = @"kSettingsAccountsTableViewId";
NSString* const kSettingsAccountsTableViewAddAccountCellId =
    @"kSettingsAccountsTableViewAddAccountCellId";
NSString* const kSettingsAccountsTableViewSignoutCellId =
    @"kSettingsAccountsTableViewSignoutCellId";
NSString* const kSettingsAccountsTableViewSyncCellId =
    @"kSettingsAccountsTableViewSyncCellId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAccounts = kSectionIdentifierEnumZero,
  SectionIdentifierSync,
  SectionIdentifierSignOut,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAccount = kItemTypeEnumZero,
  ItemTypeAddAccount,
  ItemTypeSignOut,
  ItemTypeHeader,
};

}  // namespace

@interface AccountsTableViewController () <
    ChromeIdentityServiceObserver,
    ChromeIdentityBrowserOpener,
    IdentityManagerObserverBridgeDelegate> {
  Browser* _browser;
  BOOL _closeSettingsOnAddAccount;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Modal alert for sign out.
  AlertCoordinator* _alertCoordinator;
  // Whether an authentication operation is in progress (e.g switch accounts,
  // sign out).
  BOOL _authenticationOperationInProgress;
  // Whether the view controller is currently being dismissed and new dismiss
  // requests should be ignored.
  BOOL _isBeingDismissed;
  ios::DismissASMViewControllerBlock _dimissAccountDetailsViewControllerBlock;
  ResizedAvatarCache* _avatarCache;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;

  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, TableViewItem*>* _identityMap;
}

// The SigninInteractionCoordinator that presents Sign In UI for the Accounts
// Settings page.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;

// Stops observing browser state services. This is required during the shutdown
// phase to avoid observing services for a browser state that is being killed.
- (void)stopBrowserStateServiceObservers;

@end

@implementation AccountsTableViewController

@synthesize dispatcher = _dispatcher;
@synthesize signinInteractionCoordinator = _signinInteractionCoordinator;

- (instancetype)initWithBrowser:(Browser*)browser
      closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
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

- (void)settingsWillBeDismissed {
  [self.signinInteractionCoordinator cancel];
  [_alertCoordinator stop];
  [self stopBrowserStateServiceObservers];
}

#pragma mark - SettingsRootTableViewController

- (void)reloadData {
  if (![self authService] -> IsAuthenticated()) {
    // This accounts table view will be popped or dismissed when the user
    // is signed out. Avoid reloading it in that case as that would lead to an
    // empty table view.
    return;
  }
  [super reloadData];
}

- (void)loadModel {
  // Update the title with the name with the currently signed-in account.
  ChromeIdentity* authenticatedIdentity =
      [self authService] -> GetAuthenticatedIdentity();
  NSString* title = nil;
  if (authenticatedIdentity) {
    title = [authenticatedIdentity userFullName];
    if (!title) {
      title = [authenticatedIdentity userEmail];
    }
  }
  self.title = title;

  [super loadModel];

  if (![self authService] -> IsAuthenticated())
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
  for (const auto& account : identityManager->GetAccountsWithRefreshTokens()) {
    ChromeIdentity* identity = ios::GetChromeBrowserProvider()
                                   ->GetChromeIdentityService()
                                   ->GetIdentityWithGaiaID(account.gaia);
    TableViewItem* item = [self accountItem:identity];
    [model addItem:item toSectionWithIdentifier:SectionIdentifierAccounts];

    [mutableIdentityMap setObject:item forKey:identity.gaiaID];
  }
  _identityMap = mutableIdentityMap;

  [model addItem:[self addAccountItem]
      toSectionWithIdentifier:SectionIdentifierAccounts];

  // Sign out section.
  [model addSectionWithIdentifier:SectionIdentifierSignOut];
  [model addItem:[self signOutItem]
      toSectionWithIdentifier:SectionIdentifierSignOut];
}

#pragma mark - Model objects

- (TableViewTextHeaderFooterItem*)header {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_DESCRIPTION);
  return header;
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
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeSignOut];
  item.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC);
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSettingsAccountsTableViewSignoutCellId;
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeAccount: {
      TableViewAccountItem* item =
          base::mac::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      DCHECK(item.chromeIdentity);
      [self showAccountDetails:item.chromeIdentity];
      break;
    }
    case ItemTypeAddAccount:
      [self showAddAccount];
      break;
    case ItemTypeSignOut:
      [self showDisconnect];
      break;
    default:
      break;
  }

  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  [self reloadData];
  [self popViewIfSignedOut];
  if (![self authService] -> IsAuthenticated() &&
                                 _dimissAccountDetailsViewControllerBlock) {
    _dimissAccountDetailsViewControllerBlock(/*animated=*/YES);
    _dimissAccountDetailsViewControllerBlock = nil;
  }
}

#pragma mark - Authentication operations

- (void)showAddAccount {
  if ([_alertCoordinator isVisible])
    return;

  if (!self.signinInteractionCoordinator) {
    self.signinInteractionCoordinator =
        [[SigninInteractionCoordinator alloc] initWithBrowser:_browser
                                                   dispatcher:self.dispatcher];
  }

  // |_authenticationOperationInProgress| is reset when the signin operation is
  // completed.
  _authenticationOperationInProgress = YES;
  __weak AccountsTableViewController* weakSelf = self;
  [self.signinInteractionCoordinator
      addAccountWithAccessPoint:signin_metrics::AccessPoint::
                                    ACCESS_POINT_SETTINGS
                    promoAction:signin_metrics::PromoAction::
                                    PROMO_ACTION_NO_SIGNIN_PROMO
       presentingViewController:self.navigationController
                     completion:^(BOOL success) {
                       [weakSelf handleDidAddAccount:success];
                     }];
}

- (void)handleDidAddAccount:(BOOL)success {
  [self handleAuthenticationOperationDidFinish];
  if (success && _closeSettingsOnAddAccount) {
    [self.dispatcher closeSettingsUI];
  }
}

- (void)showAccountDetails:(ChromeIdentity*)identity {
  if ([_alertCoordinator isVisible])
    return;
  _dimissAccountDetailsViewControllerBlock =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->PresentAccountDetailsController(identity, self, /*animated=*/YES);
}

- (void)showDisconnect {
  if (_authenticationOperationInProgress || [_alertCoordinator isVisible] ||
      self != [self.navigationController topViewController]) {
    // An action is already in progress, ignore user's request.
    return;
  }

  NSString* title = l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_TITLE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_INFO_MOBILE);
  NSString* continueButtonTitle =
      l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  if ([self authService] -> IsAuthenticatedIdentityManaged()) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browser->GetBrowserState());
    base::Optional<AccountInfo> accountInfo =
        identityManager->FindExtendedAccountInfoForAccountWithRefreshToken(
            identityManager->GetPrimaryAccountInfo());
    std::string hosted_domain = accountInfo.has_value()
                                    ? accountInfo.value().hosted_domain
                                    : std::string();
    title =
        l10n_util::GetNSString(IDS_IOS_MANAGED_DISCONNECT_DIALOG_TITLE_UNITY);
    message =
        l10n_util::GetNSStringF(IDS_IOS_MANAGED_DISCONNECT_DIALOG_INFO_UNITY,
                                base::UTF8ToUTF16(hosted_domain));
    continueButtonTitle =
        l10n_util::GetNSString(IDS_IOS_MANAGED_DISCONNECT_DIALOG_ACCEPT_UNITY);
  } else {
    title = l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_TITLE_UNITY);
    message =
        l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_INFO_MOBILE_UNITY);
    continueButtonTitle = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  }
  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self
                                                     title:title
                                                   message:message];

  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               action:nil
                                style:UIAlertActionStyleCancel];
  __weak AccountsTableViewController* weakSelf = self;
  [_alertCoordinator addItemWithTitle:continueButtonTitle
                               action:^{
                                 [weakSelf handleDisconnect];
                               }
                                style:UIAlertActionStyleDefault];
  [_alertCoordinator start];
}

- (void)handleDisconnect {
  AuthenticationService* authService = [self authService];
  if (authService->IsAuthenticated()) {
    _authenticationOperationInProgress = YES;
    [self preventUserInteraction];
    authService->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, ^{
      [self allowUserInteraction];
      _authenticationOperationInProgress = NO;
      [base::mac::ObjCCastStrict<SettingsNavigationController>(
          self.navigationController)
          popViewControllerOrCloseSettingsAnimated:YES];
    });
  }
}

// Sets |_authenticationOperationInProgress| to NO and pops this accounts
// table view controller if the user is signed out.
- (void)handleAuthenticationOperationDidFinish {
  DCHECK(_authenticationOperationInProgress);
  _authenticationOperationInProgress = NO;
  [self popViewIfSignedOut];
}

- (void)popViewIfSignedOut {
  if ([self authService] -> IsAuthenticated()) {
    return;
  }
  if (_authenticationOperationInProgress) {
    // The signed out state might be temporary (e.g. account switch, ...).
    // Don't pop this view based on intermediary values.
    return;
  }
  [self dismissSelfAnimated:NO];
}

- (void)dismissSelfAnimated:(BOOL)animated {
  if (_isBeingDismissed) {
    return;
  }
  _isBeingDismissed = YES;
  [self.signinInteractionCoordinator cancelAndDismiss];
  [_alertCoordinator stop];
  [self.navigationController popToViewController:self animated:NO];
  [base::mac::ObjCCastStrict<SettingsNavigationController>(
      self.navigationController)
      popViewControllerOrCloseSettingsAnimated:animated];
}

#pragma mark - Access to authentication service

- (AuthenticationService*)authService {
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

@end

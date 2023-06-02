// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAccounts = kSectionIdentifierEnumZero,
};

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAccount = kItemTypeEnumZero,
};

}  // namespace

@interface SignedInAccountsTableViewController () <
    ChromeAccountManagerServiceObserver>
@end

@implementation SignedInAccountsTableViewController {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, TableViewIdentityItem*>* _identityMap;
  // Account manager service to retrieve Chrome identities.
  ChromeAccountManagerService* _accountManagerService;
  signin::IdentityManager* _identityManager;
}

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                  accountManagerService:
                      (ChromeAccountManagerService*)accountManagerService {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver.reset(
        new ChromeAccountManagerServiceObserverBridge(self,
                                                      _accountManagerService));
  }
  return self;
}

- (void)teardownUI {
  _accountManagerServiceObserver.reset();
  _accountManagerService = nullptr;
  _identityManager = nullptr;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  self.tableView.allowsSelection = NO;
  self.tableView.backgroundColor = [UIColor clearColor];
  [self loadModel];
}

#pragma mark ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierAccounts];

  NSMutableDictionary<NSString*, TableViewIdentityItem*>* mutableIdentityMap =
      [[NSMutableDictionary alloc] init];

  for (const auto& account : _identityManager->GetAccountsWithRefreshTokens()) {
    id<SystemIdentity> identity =
        _accountManagerService->GetIdentityWithGaiaID(account.gaia);

    // If the account with a refresh token is invalidated during this operation
    // then `identity` will be nil. Do not process it in this case.
    if (!identity) {
      continue;
    }
    TableViewIdentityItem* item = [self accountItem:identity];
    [model addItem:item toSectionWithIdentifier:SectionIdentifierAccounts];
    [mutableIdentityMap setObject:item forKey:identity.gaiaID];
  }
  _identityMap = mutableIdentityMap;
}

#pragma mark ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  TableViewIdentityItem* item =
      base::mac::ObjCCastStrict<TableViewIdentityItem>(
          [_identityMap objectForKey:identity.gaiaID]);
  [self updateAccountItem:item withIdentity:identity];
  [self reconfigureCellsForItems:@[ item ]];
}

#pragma mark Private

// Creates an item and sets all the values based on `identity`.
- (TableViewIdentityItem*)accountItem:(id<SystemIdentity>)identity {
  TableViewIdentityItem* item =
      [[TableViewIdentityItem alloc] initWithType:ItemTypeAccount];
  item.identityViewStyle = IdentityViewStyleIdentityChooser;
  [self updateAccountItem:item withIdentity:identity];
  return item;
}

// Updates an item based on `identity`.
- (void)updateAccountItem:(TableViewIdentityItem*)item
             withIdentity:(id<SystemIdentity>)identity {
  item.gaiaID = identity.gaiaID;
  item.name = identity.userFullName;
  item.email = identity.userEmail;
  item.avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Regular);
}

@end

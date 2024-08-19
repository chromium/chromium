// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_model_identity_data_source.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mutator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/identity_view_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, EditAccountListSectionIdentifier) {
  SectionIdentifierPrimaryAccount = kSectionIdentifierEnumZero,
  SectionIdentifierSecondaryAccount,
};

typedef NS_ENUM(NSInteger, EditAccountListItemType) {
  // Account item.
  ItemTypeAccount = kItemTypeEnumZero,
  // Remove account item.
  ItemTypeRemoveAccount,
};

}  // namespace

@interface AccountsTableViewController () {
  // Whether to close the view after adding an account.
  BOOL _closeSettingsOnAddAccount;

  // ApplicationCommands handler.
  id<ApplicationCommands> _applicationHandler;

  // Enable lookup of item corresponding to a given IdentityViewItem GAIA ID
  // string.
  NSDictionary<NSString*, TableViewItem*>* _identityMap;
}

@end

@implementation AccountsTableViewController

@synthesize modelIdentityDataSource;

- (instancetype)initWithCloseSettingsOnAddAccount:
                    (BOOL)closeSettingsOnAddAccount
                       applicationCommandsHandler:
                           (id<ApplicationCommands>)applicationCommandsHandler {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _applicationHandler = applicationCommandsHandler;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kSettingsEditAccountListTableViewId;

  [self loadModel];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  self.title = l10n_util::GetNSString(IDS_IOS_ACCOUNT_MENU_EDIT_ACCOUNT_LIST);

  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  NSMutableDictionary<NSString*, TableViewItem*>* mutableIdentityMap =
      [[NSMutableDictionary alloc] init];

  NSString* authenticatedEmail =
      [self.modelIdentityDataSource primaryIdentityViewItem].userEmail;

  NSInteger nextSection = SectionIdentifierSecondaryAccount;

  for (const auto& identityViewItem :
       [self.modelIdentityDataSource identityViewItems]) {
    CHECK(identityViewItem);

    TableViewItem* accountItem =
        [self accountItemWithIdentityViewItem:identityViewItem];
    TableViewItem* removeAccountItem =
        [self removeAccountItemWithIdentityViewItem:identityViewItem];
    NSInteger sectionNumber;
    if ([identityViewItem.userEmail isEqualToString:authenticatedEmail]) {
      [model insertSectionWithIdentifier:SectionIdentifierPrimaryAccount
                                 atIndex:0];
      sectionNumber = SectionIdentifierPrimaryAccount;
    } else {
      [model addSectionWithIdentifier:nextSection];
      sectionNumber = nextSection++;
    }
    [model addItem:accountItem toSectionWithIdentifier:sectionNumber];
    [model addItem:removeAccountItem toSectionWithIdentifier:sectionNumber];
    [mutableIdentityMap setObject:accountItem forKey:identityViewItem.gaiaID];
  }
  _identityMap = mutableIdentityMap;
}

#pragma mark - Model objects

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
  item.accessibilityIdentifier = identityViewItem.accessibilityIdentifier;
  item.mode = TableViewAccountModeNonTappable;
  item.accessoryType = UITableViewCellAccessoryNone;
}

- (TableViewItem*)removeAccountItemWithIdentityViewItem:
    (IdentityViewItem*)identityViewItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeRemoveAccount];
  item.text = l10n_util::GetNSString(IDS_IOS_REMOVE_GOOGLE_ACCOUNT_TITLE);
  item.textColor = [UIColor colorNamed:kBlueColor];
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  EditAccountListItemType itemType = static_cast<EditAccountListItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  switch (itemType) {
    case ItemTypeAccount:
      return;
    case ItemTypeRemoveAccount:
      NSIndexPath* accountItemIndexPath =
          [NSIndexPath indexPathForRow:0 inSection:indexPath.section];
      TableViewAccountItem* accountItem =
          base::apple::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:accountItemIndexPath]);
      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self.mutator
          requestRemoveIdentityWithGaiaID:[self
                                              gaiaIDWithAccountItem:accountItem]
                                 itemView:itemView];
      break;
  }

  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - AccountsConsumer

- (void)reloadAllItems {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self reloadData];
}

- (void)updateErrorSectionModelAndReloadViewIfNeeded:(BOOL)reloadViewIfNeeded {
  return;
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

- (void)popView {
  // TODO (crbug.com/349071402): implement popView on sign-out.
}

#pragma mark - AccountsConsumer

- (NSString*)gaiaIDWithAccountItem:(TableViewItem*)accountItem {
  for (NSString* gaiaID in _identityMap) {
    if ([_identityMap[gaiaID] isEqual:accountItem]) {
      return gaiaID;
    }
  }
  return nil;
}

@end

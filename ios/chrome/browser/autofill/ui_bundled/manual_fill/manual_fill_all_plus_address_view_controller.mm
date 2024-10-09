// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_plus_address_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_cell.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace manual_fill {

enum ManualFallbackItemType : NSInteger {
  kHeader = kItemTypeEnumZero,
  kPlusAddress
};

}  // namespace manual_fill

@implementation ManualFillAllPlusAddressViewController {
  // Search controller.
  UISearchController* _searchController;
}

- (instancetype)initWithSearchController:(UISearchController*)searchController {
  self = [super init];
  if (self) {
    _searchController = searchController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.definesPresentationContext = YES;
  _searchController.searchBar.backgroundColor = [UIColor clearColor];
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.searchBar.accessibilityIdentifier =
      manual_fill::kPlusAddressSearchBarAccessibilityIdentifier;
  self.navigationItem.searchController = _searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.title = l10n_util::GetNSString(IDS_SELECT_PLUS_ADDRESS_TITLE_IOS);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(handleDoneButton)];
  doneButton.accessibilityIdentifier =
      manual_fill::kPlusAddressDoneButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = doneButton;

  [self addHeaderItem];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case manual_fill::ManualFallbackItemType::kPlusAddress:
      // Retrieve favicons for credential cells.
      [self loadFaviconForPlusAddressCell:cell indexPath:indexPath];
      break;
    default:
      break;
  }
  return cell;
}

#pragma mark - ManualFillPlusAddressConsumer

- (void)presentPlusAddresses:
    (NSArray<ManualFillPlusAddressItem*>*)plusAddresses {
  for (ManualFillPlusAddressItem* item : plusAddresses) {
    item.type = manual_fill::ManualFallbackItemType::kPlusAddress;
  }
  [self presentDataItems:plusAddresses];
}

- (void)presentPlusAddressActions:(NSArray<ManualFillActionItem*>*)actions {
  NOTREACHED_NORETURN();
}

#pragma mark - Private

- (void)handleDoneButton {
  [self.delegate selectPlusAddressViewControllerDidTapDoneButton:self];
}

// Retrieves favicon from FaviconLoader and sets image in `cell` for plus
// addresses.
- (void)loadFaviconForPlusAddressCell:(UITableViewCell*)cell
                            indexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  ManualFillPlusAddressItem* plusAddressItem =
      base::apple::ObjCCastStrict<ManualFillPlusAddressItem>(item);
  CHECK(item);

  ManualFillPlusAddressCell* plusAddressCell =
      base::apple::ObjCCastStrict<ManualFillPlusAddressCell>(cell);

  NSString* itemIdentifier = plusAddressItem.uniqueIdentifier;
  CrURL* crurl = [[CrURL alloc] initWithGURL:plusAddressItem.faviconURL];
  [self.imageDataSource
      faviconForPageURL:crurl
             completion:^(FaviconAttributes* attributes) {
               // Only set favicon if the cell hasn't been reused.
               if ([plusAddressCell.uniqueIdentifier
                       isEqualToString:itemIdentifier]) {
                 CHECK(attributes);
                 [plusAddressCell configureWithFaviconAttributes:attributes];
               }
             }];
}

// Adds a header containing text.
- (void)addHeaderItem {
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:manual_fill::ManualFallbackItemType::kHeader];

  headerItem.text =
      l10n_util::GetNSString(IDS_SELECT_PLUS_ADDRESS_HEADER_TEXT_IOS);

  [self presentHeaderItem:headerItem];
}

@end

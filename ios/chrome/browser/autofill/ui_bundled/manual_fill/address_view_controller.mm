// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "components/plus_addresses/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_cell.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace manual_fill {

enum ManualFallbackItemType : NSInteger {
  kNoAddressesMessage = kItemTypeEnumZero,
  kPlusAddress
};

}  // namespace manual_fill

@implementation AddressViewController {
  // Addresses to be shown in the view.
  NSArray<TableViewItem*>* _addresses;

  // Plus Addresses to be shown in the view.
  NSArray<TableViewItem*>* _plusAddresses;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      manual_fill::kAddressTableViewAccessibilityIdentifier;
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

#pragma mark - ManualFillAddressConsumer

// TODO(crbug.com/40577448): look at replacing ManualFillXXXConsumer with
// ManualFillItemsConsumer.
- (void)presentAddresses:(NSArray<ManualFillAddressItem*>*)addresses {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.PresentedOptions.Profiles",
                           addresses.count);

  self.noRegularDataItemsToShowHeaderItem = nil;
  if (!addresses.count && IsKeyboardAccessoryUpgradeEnabled()) {
    TableViewTextHeaderFooterItem* textHeaderFooterItem =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:manual_fill::ManualFallbackItemType::
                             kNoAddressesMessage];
    textHeaderFooterItem.text =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_ADDRESSES);
    self.noRegularDataItemsToShowHeaderItem = textHeaderFooterItem;
  }

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressIOSManualFallbackEnabled)) {
    _addresses = (NSArray<TableViewItem*>*)addresses;
    [self presentItems];
  } else {
    [self presentDataItems:(NSArray<TableViewItem*>*)addresses];
  }
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentActionItems:actions];
}

#pragma mark - ManualFillPlusAddressConsumer

- (void)presentPlusAddresses:
    (NSArray<ManualFillPlusAddressItem*>*)plusAddresses {
  _plusAddresses = (NSArray<TableViewItem*>*)plusAddresses;
  for (ManualFillPlusAddressItem* item : _plusAddresses) {
    item.type = manual_fill::ManualFallbackItemType::kPlusAddress;
  }
  [self presentItems];
}

- (void)presentPlusAddressActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentPlusAddressActionItems:actions];
}

#pragma mark - Private

// Show items depending on the availibility of `_addresses` and
// `_plusAddresses`.
- (void)presentItems {
  NSArray<TableViewItem*>* items = nil;
  if (_addresses && _plusAddresses) {
    items = [_plusAddresses arrayByAddingObjectsFromArray:_addresses];
  } else if (_addresses) {
    items = _addresses;
  } else if (_plusAddresses) {
    items = _plusAddresses;
  }

  CHECK(items);
  [self presentDataItems:items];
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

@end

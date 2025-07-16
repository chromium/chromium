// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_item_table_view.h"

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

typedef NSDiffableDataSourceSnapshot<NSString*, NSNumber*>
    SafariDataItemSnapshot;
typedef UITableViewDiffableDataSource<NSString*, NSNumber*>
    SafariDataItemDiffableDataSource;

/// Size of the leading image for each item.
constexpr NSInteger kLeadingSymbolImagePointSize = 20;

/// The identifier for the only section in the table.
NSString* const kSafariDataItemSectionIdentifier =
    @"SafariDataItemSectionIdentifier";

/// Helper methods that converts the `SafariDataItemType` to and from a NSNumber
/// representation to be used by `SafariDataItemDiffableDataSource`.
NSNumber* GetUniqueIdentifierFromType(SafariDataItemType type) {
  return @(static_cast<NSUInteger>(type));
}
SafariDataItemType GetTypeWithUniqueIdentifier(NSNumber* identifier) {
  return static_cast<SafariDataItemType>(identifier.unsignedIntegerValue);
}

/// Returns the localized label text for the given `type`.
NSString* GetTextForItemType(SafariDataItemType type) {
  int message_id;
  switch (type) {
    case SafariDataItemType::kPasswords:
      message_id = IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_TITLE_PASSWORDS;
      break;
    case SafariDataItemType::kBookmarks:
      message_id = IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_TITLE_BOOKMARKS;
      break;
    case SafariDataItemType::kHistory:
      message_id = IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_TITLE_HISTORY;
      break;
    case SafariDataItemType::kPayment:
      message_id = IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_TITLE_CREDIT_CARDS;
      break;
  }
  return l10n_util::GetNSString(message_id);
}

/// Returns the leading icon at the start of the cell for the given `type`.
UIImage* GetImageForItemType(SafariDataItemType type) {
  NSString* symbol_name;
  switch (type) {
    case SafariDataItemType::kPasswords:
      symbol_name = kKeySymbol;
      break;
    case SafariDataItemType::kBookmarks:
      symbol_name = kBookSymbol;
      break;
    case SafariDataItemType::kHistory:
      symbol_name = kClockSymbol;
      break;
    case SafariDataItemType::kPayment:
      symbol_name = kCreditCardSymbol;
      break;
  }
  return DefaultSymbolTemplateWithPointSize(symbol_name,
                                            kLeadingSymbolImagePointSize);
}

/// Returns the description for an item before it is imported, showing `count`
/// as the number of items to be imported.
NSString* GetDescriptionForUnimportedItemTypeWithCount(SafariDataItemType type,
                                                       int count) {
  if (count == 0) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_NO_DATA);
  }
  int message_id;
  switch (type) {
    case SafariDataItemType::kPasswords:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_PASSWORDS;
      break;
    case SafariDataItemType::kBookmarks:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_BOOKMARKS;
      break;
    case SafariDataItemType::kHistory:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_HISTORY;
      break;
    case SafariDataItemType::kPayment:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_CREDIT_CARDS;
      break;
  }
  return l10n_util::GetPluralNSStringF(message_id, count);
}

/// Returns the description for the item after it is imported, showing `count`
/// as the number of items imported.
NSString* GetDescriptionForImportedItemTypeWithCount(SafariDataItemType type,
                                                     int count) {
  if (type != SafariDataItemType::kPasswords) {
    CHECK_GT(count, 0);
  }
  int message_id;
  switch (type) {
    case SafariDataItemType::kPasswords:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_PASSWORDS;
      break;
    case SafariDataItemType::kBookmarks:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_BOOKMARKS;
      break;
    case SafariDataItemType::kHistory:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_HISTORY;
      break;
    case SafariDataItemType::kPayment:
      message_id =
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_CREDIT_CARDS;
      break;
  }
  return l10n_util::GetPluralNSStringF(message_id, count);
}

/// Returns a view with a spinning activity indicator.
UIView* GetAnimatingActivityIndicator() {
  UIActivityIndicatorView* activityIndicator =
      [[UIActivityIndicatorView alloc] init];
  [activityIndicator startAnimating];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  return activityIndicator;
}

/// Returns a view with a checkmark image.
UIView* GetCheckmark() {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];
  UIImageView* checkmarkImageView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithConfiguration(kCheckmarkCircleFillSymbol,
                                                   config)];
  checkmarkImageView.tintColor = [UIColor colorNamed:kGreen500Color];
  return checkmarkImageView;
}

}  // namespace

@interface SafariDataItemTableView () <UITableViewDelegate>

@end

@implementation SafariDataItemTableView {
  /// Safari data items to be displayed in the table.
  SafariDataItem* _passwordsItem;
  SafariDataItem* _bookmarksItem;
  SafariDataItem* _paymentItem;
  SafariDataItem* _historyItem;
  /// The data source for the table view.
  SafariDataItemDiffableDataSource* _dataSource;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero style:ChromeTableViewStyle()];
  if (self) {
    self.accessibilityIdentifier =
        GetSafariDataItemTableViewAccessibilityIdentifier();
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.allowsSelection = NO;
    self.backgroundColor = [UIColor clearColor];
    self.separatorInset = UIEdgeInsetsMake(0, 60, 0, 0);
    self.delegate = self;
    /// Remove extra space from UITableViewWrapperView.
    self.directionalLayoutMargins =
        NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
    self.tableHeaderView =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
    self.tableFooterView =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
    [self createDataSource];
    RegisterTableViewCell<TableViewDetailIconCell>(self);
  }
  return self;
}

#pragma mark - Helpers

/// Helper method to establish the data source.
- (void)createDataSource {
  __weak __typeof(self) weakSelf = self;
  auto cellProvider = ^UITableViewCell*(
      UITableView* tableView, NSIndexPath* indexPath, NSNumber* identifier) {
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:identifier];
  };
  _dataSource =
      [[SafariDataItemDiffableDataSource alloc] initWithTableView:self
                                                     cellProvider:cellProvider];
}

/// Returns the cell with the properties of the `item` displayed.
- (TableViewDetailIconCell*)cellForIndexPath:(NSIndexPath*)indexPath
                              itemIdentifier:(NSNumber*)identifier {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self);
  SafariDataItem* item = [self itemForUniqueIdentifier:identifier];
  if (!item) {
    return cell;
  }
  cell.backgroundColor = [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  cell.textLabel.text = GetTextForItemType(item.type);
  [self setupDescriptionForItem:item forCell:cell];
  cell.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  [cell setIconImage:GetImageForItemType(item.type)
            tintColor:[UIColor colorNamed:kBlueColor]
      backgroundColor:UIColor.clearColor
         cornerRadius:0];
  [self setupAccessoryForItem:item forCell:cell];
  cell.accessibilityIdentifier =
      GetSafariDataItemTableViewCellAccessibilityIdentifier(indexPath.item);
  return cell;
}

/// Returns the `SafariDataItem` with the given unique identifier.
- (SafariDataItem*)itemForUniqueIdentifier:(NSNumber*)identifier {
  SafariDataItemType type = GetTypeWithUniqueIdentifier(identifier);
  switch (type) {
    case SafariDataItemType::kPasswords:
      return _passwordsItem;
    case SafariDataItemType::kBookmarks:
      return _bookmarksItem;
    case SafariDataItemType::kPayment:
      return _paymentItem;
    case SafariDataItemType::kHistory:
      return _historyItem;
  }
}

/// Updates the stored Safari data item to the given `item` based on the its
/// type.
- (void)updateItemBasedOnType:(SafariDataItem*)item {
  switch (item.type) {
    case SafariDataItemType::kPasswords:
      _passwordsItem = item;
      break;
    case SafariDataItemType::kBookmarks:
      _bookmarksItem = item;
      break;
    case SafariDataItemType::kPayment:
      _paymentItem = item;
      break;
    case SafariDataItemType::kHistory:
      _historyItem = item;
      break;
  }
}

/// Helper method that sets up the description for `item`.
- (void)setupDescriptionForItem:(SafariDataItem*)item
                        forCell:(TableViewDetailIconCell*)cell {
  NSString* description =
      item.status == SafariDataItemImportStatus::kImported
          ? GetDescriptionForImportedItemTypeWithCount(item.type, item.count)
          : GetDescriptionForUnimportedItemTypeWithCount(item.type, item.count);
  if (item.invalidCount == 0) {
    [cell setDetailText:description];
    return;
  }
  /// Concatenate string for invalid passwords.
  CHECK_EQ(item.type, SafariDataItemType::kPasswords);
  CHECK_EQ(item.status, SafariDataItemImportStatus::kImported);
  std::u16string invalidCountString = l10n_util::GetPluralStringFUTF16(
      IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_INVALID_PASSWORDS,
      item.invalidCount);
  [cell setDetailText:l10n_util::GetNSStringF(
                          IDS_CONCAT_TWO_STRINGS_WITH_PERIODS,
                          base::SysNSStringToUTF16(description),
                          invalidCountString)];
  cell.detailTextNumberOfLines = 0;
}

/// Helper method that sets up the trailing accessory for `item`.
- (void)setupAccessoryForItem:(SafariDataItem*)item
                      forCell:(TableViewDetailIconCell*)cell {
  switch (item.status) {
    case SafariDataItemImportStatus::kReady:
      /// No accessory when user has not initiated importing.
      break;
    case SafariDataItemImportStatus::kImporting:
      cell.accessoryView = GetAnimatingActivityIndicator();
      break;
    case SafariDataItemImportStatus::kImported:
      if (item.invalidCount == 0) {
        cell.accessoryView = GetCheckmark();
      } else {
        /// Use `accessoryType` instead of `accessoryView` for easier
        /// formatting; also, unlike custom accessory view, tapping on built-in
        /// accessory triggers the delegate method
        /// `tableView:accessoryButtonTappedForRowWithIndexPath:`.
        cell.accessoryView = nil;
        cell.accessoryType = UITableViewCellAccessoryDetailButton;
      }
      break;
  }
}

#pragma mark - SafariDataItemConsumer

- (void)populateItems:(NSArray<SafariDataItem*>*)items {
  NSMutableArray<NSNumber*>* itemsToPopulate = [NSMutableArray array];
  for (SafariDataItem* item in items) {
    [self updateItemBasedOnType:item];
    [itemsToPopulate addObject:GetUniqueIdentifierFromType(item.type)];
  }
  SafariDataItemSnapshot* snapshot = [[SafariDataItemSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ kSafariDataItemSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:itemsToPopulate
             intoSectionWithIdentifier:kSafariDataItemSectionIdentifier];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath {
  /// TODO(crbug.com/420703283): Show the list of un-imported passwords.
}

@end

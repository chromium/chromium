// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_item_table_view.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

/// Size of the leading image for each item.
constexpr NSInteger kLeadingSymbolImagePointSize = 20;

/// Minimum animation time the user should be in the "importing" state.
constexpr base::TimeDelta kMinImportingTime = base::Seconds(0.5);

/// The identifier for the only section in the table.
NSString* const kSafariDataItemSectionIdentifier =
    @"SafariDataItemSectionIdentifier";

/// Helper methods that converts the `SafariDataItemType` to and from a NSNumber
/// representation to be used by the data source.
NSNumber* GetUniqueIdentifierFromType(SafariDataItemType type) {
  return @(static_cast<NSUInteger>(type));
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
  UIActivityIndicatorView* activity_indicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  [activity_indicator startAnimating];
  return activity_indicator;
}

/// Returns a view with a checkmark image.
UIView* GetCheckmark() {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];
  UIImageView* checkmark = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithConfiguration(kCheckmarkCircleFillSymbol,
                                                   config)];
  checkmark.tintColor = [UIColor colorNamed:kGreen500Color];
  return checkmark;
}

}  // namespace

@implementation SafariDataItemTableView {
  /// Safari data items to be displayed in the table. The dictionary key is an
  /// NSNumber representation of the type.
  NSMutableDictionary<NSNumber*, SafariDataItem*>* _itemDictionary;
  /// The data source for the table view.
  UITableViewDiffableDataSource<NSString*, NSNumber*>* _dataSource;
  /// Number of items ready to be imported.
  int _pendingImportCount;
  /// Number of items already imported.
  int _importedCount;
  /// Whether the required time for the user to be in the `importing` state has
  /// passed.
  BOOL _minimumImportingTimePassed;
  /// Number of items in the table.
  NSInteger _itemCount;
}

- (instancetype)initWithItemCount:(NSInteger)itemCount {
  self = [super initWithFrame:CGRectZero style:ChromeTableViewStyle()];
  if (self) {
    _itemCount = itemCount;
    self.accessibilityIdentifier =
        GetSafariDataItemTableViewAccessibilityIdentifier();
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.allowsSelection = NO;
    self.backgroundColor = [UIColor clearColor];
    self.separatorInset = UIEdgeInsetsMake(0, 60, 0, 0);
    /// Remove extra space from UITableViewWrapperView.
    self.directionalLayoutMargins =
        NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
    self.tableHeaderView =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
    self.tableFooterView =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
    RegisterTableViewCell<TableViewDetailIconCell>(self);
    [self reset];
  }
  return self;
}

- (void)reset {
  _pendingImportCount = 0;
  _importedCount = 0;
  _itemDictionary = [NSMutableDictionary dictionary];
}

- (void)notifyImportStart {
  /// Put items into different arrays based on whether they will be removed or
  /// not.
  NSMutableArray<NSNumber*>* identifiersToReconfigure = [NSMutableArray array];
  NSMutableArray<NSNumber*>* identifiersToDelete = [NSMutableArray array];
  for (NSNumber* identifier in _itemDictionary.allKeys) {
    SafariDataItem* item = _itemDictionary[identifier];
    if (item.count == 0) {
      /// Remove empty item types.
      [_itemDictionary removeObjectForKey:identifier];
      [identifiersToDelete addObject:identifier];
    } else {
      [item transitionToNextStatus];
      [identifiersToReconfigure addObject:identifier];
    }
  }
  /// Update snapshot.
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:identifiersToReconfigure];
  [snapshot deleteItemsWithIdentifiers:identifiersToDelete];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  /// Start timer for the "importing" state.
  __weak SafariDataItemTableView* weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf minImportingTimeDidPass];
      }),
      kMinImportingTime);
}

#pragma mark - SafariDataItemConsumer

- (void)populateItem:(SafariDataItem*)item {
  NSNumber* itemType = GetUniqueIdentifierFromType(item.type);
  /// Item should only be populated when there is a status update.
  SafariDataItem* previousItem = _itemDictionary[itemType];
  if (previousItem) {
    CHECK_NE(item.status, previousItem.status)
        << "Updating item type " << static_cast<NSUInteger>(item.type)
        << " for status " << static_cast<NSUInteger>(item.status)
        << "multiple times";
  }
  switch (item.status) {
    case SafariDataItemImportStatus::kBlockedByPolicy:
    case SafariDataItemImportStatus::kReady:
      _itemDictionary[itemType] = item;
      _pendingImportCount++;
      CHECK_LE(_pendingImportCount, _itemCount);
      if (_pendingImportCount == _itemCount) {
        [self importPreparationDidComplete];
      }
      return;
    case SafariDataItemImportStatus::kImporting:
      NOTREACHED()
          << "Transition to importing state is handled by -notifyImportStart";
    case SafariDataItemImportStatus::kImported:
      _importedCount++;
      CHECK_LE(_importedCount, _itemCount);
      if (previousItem) {
        /// Do not update the item if this item has previously been deleted.
        _itemDictionary[itemType] = item;
        if (_minimumImportingTimePassed) {
          [self moveCellsForItemsToImportState:@[ itemType ]];
        }
      }
      return;
  }
}

#pragma mark - Helpers

/// Helper method to establish the data source.
- (void)initializeDataSource {
  /// Create the data source.
  __weak __typeof(self) weakSelf = self;
  auto cellProvider = ^UITableViewCell*(
      UITableView* tableView, NSIndexPath* indexPath, NSNumber* identifier) {
    CHECK_EQ(tableView, weakSelf);
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:identifier];
  };
  _dataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self
                                                  cellProvider:cellProvider];
  /// Retrieve first snapshot and apply.
  NSArray<NSNumber*>* sortedItemIdentifiers =
      [_itemDictionary.allKeys sortedArrayUsingSelector:@selector(compare:)];
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ kSafariDataItemSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:sortedItemIdentifiers
             intoSectionWithIdentifier:kSafariDataItemSectionIdentifier];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Returns the cell with the properties of the `item` displayed.
- (TableViewDetailIconCell*)cellForIndexPath:(NSIndexPath*)indexPath
                              itemIdentifier:(NSNumber*)identifier {
  /// Check that cells are requested only when all items are available.
  SafariDataItem* item = _itemDictionary[identifier];
  CHECK(item);
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self);
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
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

/// Helper method that sets up the description for `item`.
- (void)setupDescriptionForItem:(SafariDataItem*)item
                        forCell:(TableViewDetailIconCell*)cell {
  NSString* description;
  switch (item.status) {
    case SafariDataItemImportStatus::kReady:
    case SafariDataItemImportStatus::kImporting:
      description =
          GetDescriptionForUnimportedItemTypeWithCount(item.type, item.count);
      break;
    case SafariDataItemImportStatus::kImported:
      description =
          GetDescriptionForImportedItemTypeWithCount(item.type, item.count);
      break;
    case SafariDataItemImportStatus::kBlockedByPolicy:
      description = l10n_util::GetNSString(
          IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_BLOCKED_BY_POLICY);
      break;
  }
  if (item.invalidCount > 0) {
    /// Concatenate string for invalid passwords.
    CHECK_EQ(item.type, SafariDataItemType::kPasswords);
    CHECK_EQ(item.status, SafariDataItemImportStatus::kImported);
    std::u16string invalidCountString = l10n_util::GetPluralStringFUTF16(
        IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_INVALID_PASSWORDS,
        item.invalidCount);
    description = l10n_util::GetNSStringF(IDS_CONCAT_TWO_STRINGS_WITH_PERIODS,
                                          base::SysNSStringToUTF16(description),
                                          invalidCountString);
  }
  [cell setDetailText:description];
  cell.detailTextNumberOfLines = 0;
}

/// Helper method that sets up the trailing accessory for `item`.
- (void)setupAccessoryForItem:(SafariDataItem*)item
                      forCell:(TableViewDetailIconCell*)cell {
  switch (item.status) {
    case SafariDataItemImportStatus::kBlockedByPolicy:
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

/// Handle import preparation complete.
- (void)importPreparationDidComplete {
  for (SafariDataItem* item in _itemDictionary.allValues) {
    if (item.count + item.invalidCount > 0) {
      /// Found an item to import!
      [self initializeDataSource];
      [self.importStageTransitionHandler transitionToNextImportStage];
      return;
    }
  }
  /// No item to import.
  [self.importStageTransitionHandler resetToInitialImportStage:NO];
}

/// Invoked when minimum importing time has passed.
- (void)minImportingTimeDidPass {
  _minimumImportingTimePassed = YES;
  NSMutableArray<NSNumber*>* imported = [NSMutableArray array];
  for (NSNumber* identifier in _itemDictionary.allKeys) {
    if (_itemDictionary[identifier].status ==
        SafariDataItemImportStatus::kImported) {
      [imported addObject:identifier];
    }
  }
  [self moveCellsForItemsToImportState:imported];
}

/// Transition items with `identifiers` to import state UI.
- (void)moveCellsForItemsToImportState:(NSArray<NSNumber*>*)identifiers {
  CHECK(_minimumImportingTimePassed);
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:identifiers];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
  if (_importedCount == _itemCount) {
    [self.importStageTransitionHandler transitionToNextImportStage];
  }
}

@end

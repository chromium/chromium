// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/ui/import_data_item_table_view.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/data_import/public/accessibility_utils.h"
#import "ios/chrome/browser/data_import/public/import_data_item.h"
#import "ios/chrome/browser/data_import/ui/data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
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
NSString* const kImportDataItemSectionIdentifier =
    @"ImportDataItemSectionIdentifier";

/// Helper methods that converts the `ImportDataItemType` to and from a NSNumber
/// representation to be used by the data source.
NSNumber* GetUniqueIdentifierFromType(ImportDataItemType type) {
  return @(static_cast<NSUInteger>(type));
}

/// Returns the localized label text for the given `type`.
NSString* GetTextForItemType(ImportDataItemType type) {
  int message_id;
  switch (type) {
    case ImportDataItemType::kPasswords:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_TITLE_PASSWORDS;
      break;
    case ImportDataItemType::kBookmarks:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_TITLE_BOOKMARKS;
      break;
    case ImportDataItemType::kHistory:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_TITLE_HISTORY;
      break;
    case ImportDataItemType::kPayment:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_TITLE_CREDIT_CARDS;
      break;
    case ImportDataItemType::kPasskeys:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_TITLE_PASSKEYS;
      break;
  }
  return l10n_util::GetNSString(message_id);
}

/// Returns the leading icon at the start of the cell for the given `type`.
UIImage* GetImageForItemType(ImportDataItemType type) {
  NSString* symbol_name;
  switch (type) {
    case ImportDataItemType::kPasswords:
      symbol_name = kKeySymbol;
      break;
    case ImportDataItemType::kBookmarks:
      symbol_name = kBookSymbol;
      break;
    case ImportDataItemType::kHistory:
      symbol_name = kClockSymbol;
      break;
    case ImportDataItemType::kPayment:
      symbol_name = kCreditCardSymbol;
      break;
    case ImportDataItemType::kPasskeys:
      symbol_name = kPersonBadgeKeyFillSymbol;
      break;
  }
  return DefaultSymbolTemplateWithPointSize(symbol_name,
                                            kLeadingSymbolImagePointSize);
}

/// Returns the description for an item before it is imported, showing `count`
/// as the number of items to be imported.
NSString* GetDescriptionForUnimportedItemTypeWithCount(ImportDataItemType type,
                                                       int count) {
  if (count == 0) {
    return l10n_util::GetNSString(
        IDS_IOS_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_NO_DATA);
  }
  int message_id;
  switch (type) {
    case ImportDataItemType::kPasswords:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_PASSWORDS;
      break;
    case ImportDataItemType::kBookmarks:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_BOOKMARKS;
      break;
    case ImportDataItemType::kHistory:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_HISTORY;
      break;
    case ImportDataItemType::kPayment:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_CREDIT_CARDS;
      break;
    case ImportDataItemType::kPasskeys:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_PENDING_DETAILED_TEXT_PASSKEYS;
      break;
  }
  return l10n_util::GetPluralNSStringF(message_id, count);
}

/// Returns the description for the item after it is imported, showing `count`
/// as the number of items imported.
NSString* GetDescriptionForImportedItemTypeWithCount(ImportDataItemType type,
                                                     int count) {
  int message_id;
  switch (type) {
    case ImportDataItemType::kPasswords:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_PASSWORDS;
      break;
    case ImportDataItemType::kBookmarks:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_BOOKMARKS;
      break;
    case ImportDataItemType::kHistory:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_HISTORY;
      break;
    case ImportDataItemType::kPayment:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_CREDIT_CARDS;
      break;
    case ImportDataItemType::kPasskeys:
      message_id = IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_PASSKEYS;
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

@implementation ImportDataItemTableView {
  /// Import data items to be displayed in the table. The dictionary key is an
  /// NSNumber representation of the type.
  NSMutableDictionary<NSNumber*, ImportDataItem*>* _itemDictionary;
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
        GetImportDataItemTableViewAccessibilityIdentifier();
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

    [TableViewCellContentConfiguration registerCellForTableView:self];

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
    ImportDataItem* item = _itemDictionary[identifier];
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
  __weak ImportDataItemTableView* weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf minImportingTimeDidPass];
      }),
      kMinImportingTime);
}

#pragma mark - ImportDataItemConsumer

- (void)populateItem:(ImportDataItem*)item {
  NSNumber* itemType = GetUniqueIdentifierFromType(item.type);
  /// Item should only be populated when there is a status update.
  ImportDataItem* previousItem = _itemDictionary[itemType];
  if (previousItem) {
    CHECK_NE(item.status, previousItem.status)
        << "Updating item type " << static_cast<NSUInteger>(item.type)
        << " for status " << static_cast<NSUInteger>(item.status)
        << "multiple times";
  }
  switch (item.status) {
    case ImportDataItemImportStatus::kBlockedByPolicy:
    case ImportDataItemImportStatus::kReady:
      _itemDictionary[itemType] = item;
      _pendingImportCount++;
      CHECK_LE(_pendingImportCount, _itemCount);
      if (_pendingImportCount == _itemCount) {
        [self importPreparationDidComplete];
      }
      return;
    case ImportDataItemImportStatus::kImporting:
      NOTREACHED()
          << "Transition to importing state is handled by -notifyImportStart";
    case ImportDataItemImportStatus::kImported:
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
      appendSectionsWithIdentifiers:@[ kImportDataItemSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:sortedItemIdentifiers
             intoSectionWithIdentifier:kImportDataItemSectionIdentifier];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Returns the cell with the properties of the `item` displayed.
- (UITableViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(NSNumber*)identifier {
  /// Check that cells are requested only when all items are available.
  ImportDataItem* item = _itemDictionary[identifier];
  CHECK(item);

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = GetTextForItemType(item.type);
  configuration.subtitle = [self descriptionForItem:item];

  ColorfulSymbolContentConfiguration* symbolConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  symbolConfiguration.symbolImage = GetImageForItemType(item.type);
  symbolConfiguration.symbolTintColor = [UIColor colorNamed:kBlueColor];

  configuration.leadingConfiguration = symbolConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:self];
  cell.contentConfiguration = configuration;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  [self setupAccessoryForItem:item forCell:cell];
  cell.accessibilityIdentifier =
      GetImportDataItemTableViewCellAccessibilityIdentifier(indexPath.item);
  return cell;
}

/// Returns the description for `item`.
- (NSString*)descriptionForItem:(ImportDataItem*)item {
  NSString* description;
  switch (item.status) {
    case ImportDataItemImportStatus::kReady:
    case ImportDataItemImportStatus::kImporting:
      description =
          GetDescriptionForUnimportedItemTypeWithCount(item.type, item.count);
      break;
    case ImportDataItemImportStatus::kImported:
      description =
          GetDescriptionForImportedItemTypeWithCount(item.type, item.count);
      break;
    case ImportDataItemImportStatus::kBlockedByPolicy:
      description =
          l10n_util::GetNSString(IDS_IOS_IMPORT_ITEM_BLOCKED_BY_POLICY);
      break;
  }
  if (item.invalidCount > 0) {
    /// Concatenate string for invalid passwords.
    CHECK_EQ(item.type, ImportDataItemType::kPasswords);
    CHECK_EQ(item.status, ImportDataItemImportStatus::kImported);
    std::u16string invalidCountString = l10n_util::GetPluralStringFUTF16(
        IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_INVALID_PASSWORDS,
        item.invalidCount);
    description = l10n_util::GetNSStringF(IDS_CONCAT_TWO_STRINGS_WITH_PERIODS,
                                          base::SysNSStringToUTF16(description),
                                          invalidCountString);
  }
  return description;
}

/// Helper method that sets up the trailing accessory for `item`.
- (void)setupAccessoryForItem:(ImportDataItem*)item
                      forCell:(UITableViewCell*)cell {
  switch (item.status) {
    case ImportDataItemImportStatus::kBlockedByPolicy:
    case ImportDataItemImportStatus::kReady:
      /// No accessory when user has not initiated importing.
      break;
    case ImportDataItemImportStatus::kImporting:
      cell.accessoryView = GetAnimatingActivityIndicator();
      break;
    case ImportDataItemImportStatus::kImported:
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
  BOOL shouldInitiateImport = NO;
  BOOL anyTypeBlockedByPolicy = NO;

  for (ImportDataItem* item in _itemDictionary.allValues) {
    if (item.status == ImportDataItemImportStatus::kBlockedByPolicy) {
      anyTypeBlockedByPolicy = YES;
    } else if (item.status == ImportDataItemImportStatus::kReady &&
               (item.count + item.invalidCount > 0)) {
      shouldInitiateImport = YES;
      break;
    }
  }

  if (shouldInitiateImport) {
    [self initializeDataSource];
    [self.importStageTransitionHandler transitionToNextImportStage];
  } else if (anyTypeBlockedByPolicy) {
    [self.importStageTransitionHandler
        resetToInitialImportStage:DataImportResetReason::
                                      kAllDataBlockedByPolicy];
  } else {
    [self.importStageTransitionHandler
        resetToInitialImportStage:DataImportResetReason::kNoImportableData];
  }
}

/// Invoked when minimum importing time has passed.
- (void)minImportingTimeDidPass {
  _minimumImportingTimePassed = YES;
  NSMutableArray<NSNumber*>* imported = [NSMutableArray array];
  for (NSNumber* identifier in _itemDictionary.allKeys) {
    if (_itemDictionary[identifier].status ==
        ImportDataItemImportStatus::kImported) {
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

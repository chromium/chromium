// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  HeaderSectionIdentifier = kSectionIdentifierEnumZero,
  NoDataItemsSectionIdentifier,
  ActionsSectionIdentifier,
  PlusAddressActionsSectionIdentifier,
  // Must be declared last as it is used as the starting point to dynamically
  // create section identifiers for each data item when the
  // kIOSKeyboardAccessoryUpgrade feature is enabled.
  DataItemsSectionIdentifier
};

namespace {

// This is the width used for `self.preferredContentSize`.
constexpr CGFloat PopoverPreferredWidth = 320;

// This is the height used for `self.preferredContentSize` when showing the
// loading indicator on iPad.
constexpr CGFloat PopoverLoadingHeight = 185.5;

// Minimum and maximum heights permitted for `self.preferredContentSize`.
constexpr CGFloat PopoverMinHeight = 160;
constexpr CGFloat PopoverMaxHeight = 360;

// If the loading indicator was shown, it will be on screen for at least this
// duration.
constexpr base::TimeDelta kMinimumLoadingTime = base::Milliseconds(500);

// Height of the section header.
constexpr CGFloat kSectionHeaderHeight = 6;

// Height of the section footer.
constexpr CGFloat kSectionFooterHeight = 8;

// Left inset of the table view's section separators.
constexpr CGFloat kSectionSepatatorLeftInset = 16;

// Represents the different types of items that can be presented.
enum class ItemType {
  kItemTypeData = kItemTypeEnumZero,
  kItemTypeAction,
  kItemTypePlusAddressAction
};

}  // namespace

@interface FallbackViewController ()
// Header item to be shown when the loading indicator disappears.
@property(nonatomic, strong) TableViewHeaderFooterItem* queuedHeaderItem;

// Data Items to be shown when the loading indicator disappears.
@property(nonatomic, strong) NSArray<TableViewItem*>* queuedDataItems;

// Action Items to be shown when the loading indicator disappears.
@property(nonatomic, strong) NSArray<TableViewItem*>* queuedActionItems;

// Plus Address Action Items to be shown when the loading indicator disappears.
@property(nonatomic, strong)
    NSArray<TableViewItem*>* queuedPlusAddressActionItems;

@end

@implementation FallbackViewController {
  // The time when the loading indicator started.
  base::Time _loadingIndicatorStartingTime;

  // The number of data items that are currently being presented.
  NSInteger _dataItemCount;
}

- (instancetype)init {
  self = [super initWithStyle:IsKeyboardAccessoryUpgradeEnabled()
                                  ? ChromeTableViewStyle()
                                  : UITableViewStylePlain];

  if (self) {
    _loadingIndicatorStartingTime = base::Time::Min();
  }

  return self;
}

- (void)viewDidLoad {
  // Super's `viewDidLoad` uses `styler.tableViewBackgroundColor` so it needs to
  // be set before.
  self.styler.tableViewBackgroundColor =
      [UIColor colorNamed:IsKeyboardAccessoryUpgradeEnabled()
                              ? kGroupedPrimaryBackgroundColor
                              : kBackgroundColor];

  [super viewDidLoad];

  // Remove extra spacing on top of sections.
  self.tableView.sectionHeaderTopPadding = 0;

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    self.tableView.separatorInset =
        UIEdgeInsetsMake(0, kSectionSepatatorLeftInset, 0, 0);
  } else {
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    self.tableView.separatorInset = UIEdgeInsetsMake(0, 0, 0, 0);
  }
  self.tableView.estimatedRowHeight = 1;
  self.tableView.allowsSelection = NO;
  self.definesPresentationContext = YES;
  if (!self.tableViewModel) {
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      self.preferredContentSize = CGSizeMake(
          PopoverPreferredWidth, AlignValueToPixel(PopoverLoadingHeight));
    }
    [self startLoadingIndicatorWithLoadingMessage:@""];
    _loadingIndicatorStartingTime = base::Time::Now();
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    CGSize systemLayoutSize = self.tableView.contentSize;
    CGFloat preferredHeight =
        std::min(systemLayoutSize.height, PopoverMaxHeight);
    preferredHeight = std::max(preferredHeight, PopoverMinHeight);
    self.preferredContentSize =
        CGSizeMake(PopoverPreferredWidth, preferredHeight);
  }
}

- (void)presentHeaderItem:(TableViewHeaderFooterItem*)item {
  if (![self shouldPresentItems]) {
    if (self.queuedHeaderItem) {
      self.queuedHeaderItem = item;
      return;
    }
    self.queuedHeaderItem = item;
    __weak __typeof(self) weakSelf = self;
    [self presentItemsAfterMinimumLoadingTime:^{
      [weakSelf presentQueuedHeaderItem];
    }];
    return;
  }
  self.queuedHeaderItem = item;
  [self presentQueuedHeaderItem];
}

- (void)presentDataItems:(NSArray<TableViewItem*>*)items {
  [self presentItems:items ofItemType:ItemType::kItemTypeData];
}

- (void)presentActionItems:(NSArray<TableViewItem*>*)actions {
  [self presentItems:actions ofItemType:ItemType::kItemTypeAction];
}

- (void)presentPlusAddressActionItems:(NSArray<TableViewItem*>*)actions {
  [self presentItems:actions ofItemType:ItemType::kItemTypePlusAddressAction];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.tableViewModel headerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }
  return kSectionHeaderHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (self.noRegularDataItemsToShowHeaderItem &&
      [self.tableViewModel
          hasSectionForSectionIdentifier:NoDataItemsSectionIdentifier] &&
      section ==
          [self.tableViewModel
              sectionForSectionIdentifier:NoDataItemsSectionIdentifier]) {
    return 0;
  }

  if ([self.tableViewModel footerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }
  return kSectionFooterHeight;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* headerView = [super tableView:tableView
                 viewForHeaderInSection:section];

  // Set the font and text color of the text label for headers of type
  // TableViewTextHeaderFooterView.
  if ([headerView isKindOfClass:[TableViewTextHeaderFooterView class]]) {
    TableViewTextHeaderFooterView* textHeaderView =
        base::apple::ObjCCastStrict<TableViewTextHeaderFooterView>(headerView);
    textHeaderView.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    textHeaderView.textLabel.textColor =
        [UIColor colorNamed:kTextSecondaryColor];
  }

  return headerView;
}

#pragma mark - Private

// Presents an array of TableViewItems, handling queuing and delayed
// presentation if necessary.
- (void)presentItems:(NSArray<TableViewItem*>*)items
          ofItemType:(ItemType)itemType {
  BOOL hasQueuedItems = NO;

  // Queue the items based on their type.
  switch (itemType) {
    case ItemType::kItemTypeData:
      hasQueuedItems = (self.queuedDataItems != nil);
      self.queuedDataItems = items;
      break;
    case ItemType::kItemTypeAction:
      hasQueuedItems = (self.queuedActionItems != nil);
      self.queuedActionItems = items;
      break;
    case ItemType::kItemTypePlusAddressAction:
      hasQueuedItems = (self.queuedPlusAddressActionItems != nil);
      self.queuedPlusAddressActionItems = items;
      break;
  }

  if (![self shouldPresentItems]) {
    if (hasQueuedItems) {
      return;
    }

    // Delay presentation until after minimum loading time.
    __weak __typeof(self) weakSelf = self;
    [self presentItemsAfterMinimumLoadingTime:^{
      [weakSelf presentQueuedItemsOfType:itemType];
    }];
    return;
  }

  [self presentQueuedItemsOfType:itemType];
}

// Presents the queued items based on their type.
- (void)presentQueuedItemsOfType:(ItemType)itemType {
  switch (itemType) {
    case ItemType::kItemTypeData:
      [self presentQueuedDataItems];
      break;
    case ItemType::kItemTypeAction:
      [self presentQueuedActionItems];
      break;
    case ItemType::kItemTypePlusAddressAction:
      [self presentQueuedPlusAddressActionItems];
      break;
  }
}

// Calls `presentationBlock` to update the items in `tableView` after
// `kMinimumLoadingTime` has passed.
- (void)presentItemsAfterMinimumLoadingTime:(void (^)(void))presentationBlock {
  const base::TimeDelta remainingTime =
      kMinimumLoadingTime - [self timeSinceLoadingIndicatorStarted];
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(presentationBlock), remainingTime);
}

// Presents the header item currently in queue.
- (void)presentQueuedHeaderItem {
  [self createModelIfNeeded];

  BOOL sectionExists = [self.tableViewModel
      hasSectionForSectionIdentifier:HeaderSectionIdentifier];
  // If there is no header, remove section if it exists.
  if (!self.queuedHeaderItem && sectionExists) {
    [self.tableViewModel removeSectionWithIdentifier:HeaderSectionIdentifier];
  } else if (self.queuedHeaderItem) {
    if (!sectionExists) {
      [self.tableViewModel insertSectionWithIdentifier:HeaderSectionIdentifier
                                               atIndex:0];
    }
    [self.tableViewModel setHeader:self.queuedHeaderItem
          forSectionWithIdentifier:HeaderSectionIdentifier];
  }
  [self.tableView reloadData];
  self.queuedHeaderItem = nil;
}

// Presents the data items currently in queue.
- (void)presentQueuedDataItems {
  DCHECK(self.queuedDataItems);

  [self createModelIfNeeded];

  [self updateEmptyStateMessage];

  BOOL sectionExists = [self.tableViewModel
      hasSectionForSectionIdentifier:DataItemsSectionIdentifier];

  // Determine the index at which the next section should be inserted based on
  // header existance.
  NSInteger sectionIndex =
      [self.tableViewModel
          hasSectionForSectionIdentifier:HeaderSectionIdentifier]
          ? 1
          : 0;

  // If the kIOSKeyboardAccessoryUpgrade feature is enabled, remove any excess
  // data item sections, and present the queued data items.
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    [self removeUnusedDataItemSections];
    [self presentFallbackItems:self.queuedDataItems
             startingAtSection:DataItemsSectionIdentifier
               startingAtIndex:sectionIndex];
    _dataItemCount = self.queuedDataItems.count;
  } else {
    if (!self.queuedDataItems.count && sectionExists) {
      [self.tableViewModel
          removeSectionWithIdentifier:DataItemsSectionIdentifier];
    } else if (self.queuedDataItems.count && !sectionExists) {
      [self.tableViewModel
          insertSectionWithIdentifier:DataItemsSectionIdentifier
                              atIndex:sectionIndex];
    }
    [self presentFallbackItems:self.queuedDataItems
                     inSection:DataItemsSectionIdentifier];
  }
  self.queuedDataItems = nil;
}

// Presents the action items currently in queue.
- (void)presentQueuedActionItems {
  [self presentActionItems:self.queuedActionItems
                 inSection:ActionsSectionIdentifier];
  self.queuedActionItems = nil;
}

// Presents plus address action items currently in the queue.
- (void)presentQueuedPlusAddressActionItems {
  [self presentActionItems:self.queuedPlusAddressActionItems
                 inSection:PlusAddressActionsSectionIdentifier];
  self.queuedPlusAddressActionItems = nil;
}

// Presents action items `items` in the `section`.
- (void)presentActionItems:(NSArray<TableViewItem*>*)items
                 inSection:(SectionIdentifier)section {
  CHECK(items);

  [self createModelIfNeeded];

  BOOL sectionExists =
      [self.tableViewModel hasSectionForSectionIdentifier:section];
  // If there are no passed items, remove section if it exists.
  if (!items.count && sectionExists) {
    [self.tableViewModel removeSectionWithIdentifier:section];
  } else if (items.count && !sectionExists) {
    [self.tableViewModel addSectionWithIdentifier:section];
  }

  [self presentFallbackItems:items inSection:section];
}

// Returns the time elapsed in seconds since the loading indicator started. This
// is >= `kMinimumLoadingTime` if the loading indicator wasn't shown.
- (base::TimeDelta)timeSinceLoadingIndicatorStarted {
  return base::Time::Now() - _loadingIndicatorStartingTime;
}

// Indicates if the view is ready for data to be presented.
- (BOOL)shouldPresentItems {
  return [self timeSinceLoadingIndicatorStarted] >= kMinimumLoadingTime;
}

// Creates the table view model if not created already.
- (void)createModelIfNeeded {
  if (!self.tableViewModel) {
    [self loadModel];
    [self stopLoadingIndicatorWithCompletion:nil];
  }
}

// Presents `items` in the respective section.
- (void)presentFallbackItems:(NSArray<TableViewItem*>*)items
                   inSection:(SectionIdentifier)sectionIdentifier {
  if (items.count) {
    [self.tableViewModel
        deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
    for (TableViewItem* item in items) {
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:sectionIdentifier];
    }
  }
  [self.tableView reloadData];
}

// Presents `items` in individual subsequent sections. New section identifiers
// are generated sequentially starting from 'sectionIdentifier', and sections
// are inserted beginning at the given 'index'.
- (void)presentFallbackItems:(NSArray<TableViewItem*>*)items
           startingAtSection:(NSInteger)sectionIdentifier
             startingAtIndex:(NSInteger)index {
  for (TableViewItem* item in items) {
    // If the section already exists, remove all of its objects. Otherwise,
    // create it.
    if ([self.tableViewModel
            hasSectionForSectionIdentifier:sectionIdentifier]) {
      [self.tableViewModel
          deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
    } else {
      [self.tableViewModel insertSectionWithIdentifier:sectionIdentifier
                                               atIndex:index];
    }
    [self.tableViewModel addItem:item
         toSectionWithIdentifier:sectionIdentifier];
    sectionIdentifier++;
    index++;
  }
  [self.tableView reloadData];
}

// Removes all data item sections that were created and that are not needed
// anymore.
- (void)removeUnusedDataItemSections {
  int numberOfSectionsToDelete = _dataItemCount - self.queuedDataItems.count;
  if (numberOfSectionsToDelete <= 0) {
    return;
  }

  int lastSectionToDelete = _dataItemCount - 1;
  int firstSectionToDelete = lastSectionToDelete - numberOfSectionsToDelete + 1;
  for (int i = firstSectionToDelete; i <= lastSectionToDelete; i++) {
    NSInteger sectionIdentifier = DataItemsSectionIdentifier + i;
    if ([self.tableViewModel
            hasSectionForSectionIdentifier:sectionIdentifier]) {
      [self.tableViewModel removeSectionWithIdentifier:sectionIdentifier];
    }
  }
}

// Adds or removes the `noRegularDataItemsToShowHeaderItem` if needed. This
// header item is displayed to let the user know that there are no data items
// amongst passwords, cards and addresses to show. However, plus address can
// still be shown.
- (void)updateEmptyStateMessage {
  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    return;
  }

  BOOL needsEmptyStateHeader = self.noRegularDataItemsToShowHeaderItem;
  BOOL hasEmptyStateSection = [self.tableViewModel
      hasSectionForSectionIdentifier:NoDataItemsSectionIdentifier];
  BOOL hasEmptyStateHeader =
      hasEmptyStateSection &&
      [self.tableViewModel
          headerForSectionWithIdentifier:NoDataItemsSectionIdentifier];

  if (needsEmptyStateHeader == hasEmptyStateHeader) {
    return;
  }

  if (needsEmptyStateHeader) {
    [self.tableViewModel addSectionWithIdentifier:NoDataItemsSectionIdentifier];
    [self.tableViewModel setHeader:self.noRegularDataItemsToShowHeaderItem
          forSectionWithIdentifier:NoDataItemsSectionIdentifier];
  } else {
    [self.tableViewModel
        removeSectionWithIdentifier:NoDataItemsSectionIdentifier];
    self.noRegularDataItemsToShowHeaderItem = nil;
  }
}

@end

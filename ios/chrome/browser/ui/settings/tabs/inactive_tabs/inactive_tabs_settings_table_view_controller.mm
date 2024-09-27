// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller.h"

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Sections identifier.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierOptions = kSectionIdentifierEnumZero,
};

// Item types to enumerate the table items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeOptionsNever,
  ItemTypeOptions7Days,
  ItemTypeOptions14Days,
  ItemTypeOptions21Days,
};

// Converts an ItemType, to a corresponding inactive day threshold.
int InactiveDaysThresholdWithItemType(ItemType item_type) {
  switch (item_type) {
    case ItemTypeOptionsNever:
      return -1;
    case ItemTypeOptions7Days:
      return 7;
    case ItemTypeOptions14Days:
      return 14;
    case ItemTypeOptions21Days:
      return 21;
    case ItemTypeHeader:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

}  // namespace.

@implementation InactiveTabsSettingsTableViewController {
  // Current inactive tab days threshold.
  int _threshold;
}

#pragma mark - Initialization

- (instancetype)init {
  CHECK(IsInactiveTabsAvailable());
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_MOVE_INACTIVE_TABS);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedRowHeight = 70;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.accessibilityIdentifier = kInactiveTabsSettingsTableViewId;
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierOptions];

  // Header item.
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text =
      l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_SETTINGS_HEADER);
  [model setHeader:headerItem
      forSectionWithIdentifier:SectionIdentifierOptions];

  // Option items.
  [model addItem:[self createItemWithType:ItemTypeOptionsNever]
      toSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:[self createItemWithType:ItemTypeOptions7Days]
      toSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:[self createItemWithType:ItemTypeOptions14Days]
      toSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:[self createItemWithType:ItemTypeOptions21Days]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [self updateCheckedStateWithDaysThreshold:_threshold];
}

#pragma mark - Internal methods

// Creates a table view item for the given type.
- (TableViewDetailTextItem*)createItemWithType:(ItemType)itemType {
  TableViewDetailTextItem* tableViewItem =
      [[TableViewDetailTextItem alloc] initWithType:itemType];
  if (itemType == ItemTypeOptionsNever) {
    tableViewItem.text =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_INACTIVE_TABS_DISABLED);
  } else {
    tableViewItem.text = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD),
            InactiveDaysThresholdWithItemType(itemType)));
  }
  tableViewItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return tableViewItem;
}

#pragma mark - InactiveTabsSettingsConsumer

- (void)updateCheckedStateWithDaysThreshold:(int)threshold {
  _threshold = threshold;

  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (TableViewDetailTextItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierOptions]) {
    int itemSetting =
        InactiveDaysThresholdWithItemType(static_cast<ItemType>(item.type));

    UITableViewCellAccessoryType desiredType =
        itemSetting == _threshold ? UITableViewCellAccessoryCheckmark
                                  : UITableViewCellAccessoryNone;

    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self reconfigureCellsForItems:modifiedItems];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:NO];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  int chosenSetting = InactiveDaysThresholdWithItemType(static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]));
  [self.delegate inactiveTabsSettingsTableViewController:self
                          didSelectInactiveDaysThreshold:chosenSetting];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileInactiveTabsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileInactiveTabsSettingsBack"));
}

@end

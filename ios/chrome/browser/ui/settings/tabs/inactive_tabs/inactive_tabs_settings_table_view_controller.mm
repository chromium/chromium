// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller.h"

#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
      NOTREACHED();
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
  DCHECK(IsInactiveTabsEnabled() || IsInactiveTabsExplictlyDisabledByUser());
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
  [model addItem:[self createTextItemWithText:
                           l10n_util::GetNSString(
                               IDS_IOS_OPTIONS_INACTIVE_TABS_DISABLED)
                                         item:ItemTypeOptionsNever]
      toSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:[self createTextItemWithText:
                           l10n_util::GetNSStringF(
                               IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD, u"7")
                                         item:ItemTypeOptions7Days]
      toSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:[self createTextItemWithText:
                           l10n_util::GetNSStringF(
                               IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD, u"14")
                                         item:ItemTypeOptions14Days]
      toSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:[self createTextItemWithText:
                           l10n_util::GetNSStringF(
                               IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD, u"21")
                                         item:ItemTypeOptions21Days]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [self updateCheckedStateWithDaysThreshold:_threshold];
}

#pragma mark - Internal methods

// Creates a table view item with text and a type.
- (TableViewDetailTextItem*)createTextItemWithText:(NSString*)text
                                              item:(ItemType)item {
  TableViewDetailTextItem* tableViewItem =
      [[TableViewDetailTextItem alloc] initWithType:item];
  tableViewItem.text = text;
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
  if (@available(iOS 16.0, *)) {
    return;
  }
  int chosenSetting = InactiveDaysThresholdWithItemType(static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]));
  [self.delegate inactiveTabsSettingsTableViewController:self
                          didSelectInactiveDaysThreshold:chosenSetting];
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
  // TODO(crbug.com/1418021): Add metrics when the user go close Inactive Tabs
  // Settings.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/1418021): Add metrics when the user go back from Inactive
  // Tabs Settings to Tabs Settings screen.
}

@end

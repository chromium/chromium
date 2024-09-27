// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_table_view_controller.h"

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_table_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInactiveTabs = kSectionIdentifierEnumZero,
};

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeInactiveTabs = kItemTypeEnumZero,
};

}  // namespace

@implementation TabsSettingsTableViewController {
  // Updatable inactive tabs item.
  TableViewDetailIconItem* _inactiveTabsDetailItem;
  // Current inactive tab days threshold.
  int _inactiveDaysThreshold;
}

- (instancetype)init {
  CHECK(IsInactiveTabsAvailable());
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_TABS_MANAGEMENT_SETTINGS);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedRowHeight = 70;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.accessibilityIdentifier = kTabsSettingsTableViewId;
  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  if (IsInactiveTabsAvailable()) {
    [model addSectionWithIdentifier:SectionIdentifierInactiveTabs];
    [model addItem:[self moveInactiveTabsItem]
        toSectionWithIdentifier:SectionIdentifierInactiveTabs];
    [self updateInactiveTabsItemWithDaysThreshold:_inactiveDaysThreshold];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileTabsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileTabsSettingsBack"));
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  [self performPrimaryActionForRowAtIndexPath:indexPath];
}

#pragma mark - TabsSettingsConsumer

- (void)setInactiveTabsTimeThreshold:(int)threshold {
  [self updateInactiveTabsItemWithDaysThreshold:threshold];
}

#pragma mark - Private

// Called when a row is selected at `indexPath`.
- (void)performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  ItemType type = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  switch (type) {
    case ItemTypeInactiveTabs:
      [self.delegate
          tabsSettingsTableViewControllerDidSelectInactiveTabsSettings:self];
      break;
  }
}

// Returns a newly created TableViewDetailIconItem for the inactive tabs
// settings menu.
- (TableViewDetailIconItem*)moveInactiveTabsItem {
  _inactiveTabsDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeInactiveTabs];
  _inactiveTabsDetailItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_MOVE_INACTIVE_TABS);
  _inactiveTabsDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _inactiveTabsDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  _inactiveTabsDetailItem.accessibilityIdentifier =
      kSettingsMoveInactiveTabsCellId;
  return _inactiveTabsDetailItem;
}

// Updates the detail text for the Inactive tabs item.
- (void)updateInactiveTabsItemWithDaysThreshold:(int)threshold {
  _inactiveDaysThreshold = threshold;
  if (!_inactiveTabsDetailItem) {
    return;
  }
  if (_inactiveDaysThreshold == kInactiveTabsDisabledByUser) {
    _inactiveTabsDetailItem.detailText =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_INACTIVE_TABS_DISABLED);
  } else {
    _inactiveTabsDetailItem.detailText = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD),
            _inactiveDaysThreshold));
  }
  [self reconfigureCellsForItems:@[ _inactiveTabsDetailItem ]];
}

@end

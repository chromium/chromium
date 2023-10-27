// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_table_view_controller.h"

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
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
  SectionIdentifierTabPickup,
};

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeInactiveTabs = kItemTypeEnumZero,
  ItemTypeTabPickup,
};

}  // namespace

@implementation TabsSettingsTableViewController {
  // Updatable inactive tabs item.
  TableViewDetailIconItem* _inactiveTabsDetailItem;
  // Updatable tab pickup item.
  TableViewDetailIconItem* _tabPickupDetailItem;
  // Current inactive tab days threshold.
  int _inactiveDaysThreshold;
  // State of the tab pickup feature.
  bool _tabPickupEnabled;
}

- (instancetype)init {
  CHECK(IsInactiveTabsAvailable() || IsTabPickupEnabled());
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

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  if (IsInactiveTabsAvailable()) {
    [model addSectionWithIdentifier:SectionIdentifierInactiveTabs];
    [model addItem:[self moveInactiveTabsItem]
        toSectionWithIdentifier:SectionIdentifierInactiveTabs];
    [self updateInactiveTabsItemWithDaysThreshold:_inactiveDaysThreshold];
  }

  if (IsTabPickupEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierTabPickup];
    [model addItem:[self tabPickupItem]
        toSectionWithIdentifier:SectionIdentifierTabPickup];
    [self updateTabPickupState:_tabPickupEnabled];
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
  if (@available(iOS 16.0, *)) {
    return;
  }
  [self performPrimaryActionForRowAtIndexPath:indexPath];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  [self performPrimaryActionForRowAtIndexPath:indexPath];
}

#pragma mark - TabsSettingsConsumer

- (void)setInactiveTabsTimeThreshold:(int)threshold {
  [self updateInactiveTabsItemWithDaysThreshold:threshold];
}

- (void)setTabPickupEnabled:(bool)enabled {
  [self updateTabPickupState:enabled];
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
    case ItemTypeTabPickup:
      [self.delegate
          tabsSettingsTableViewControllerDidSelectTabPickupSettings:self];
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

// Returns a newly created TableViewDetailIconItem for the tab pickup
// settings menu.
- (TableViewDetailIconItem*)tabPickupItem {
  _tabPickupDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTabPickup];
  _tabPickupDetailItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP);
  _tabPickupDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _tabPickupDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  _tabPickupDetailItem.accessibilityIdentifier = kSettingsTabPickupCellId;
  return _tabPickupDetailItem;
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

// Updates the detail text for the tab pickup item.
- (void)updateTabPickupState:(bool)enabled {
  _tabPickupEnabled = enabled;
  if (!_tabPickupDetailItem) {
    return;
  }

  _tabPickupDetailItem.detailText = l10n_util::GetNSString(
      _tabPickupEnabled ? IDS_IOS_SETTING_ON : IDS_IOS_SETTING_OFF);
  [self reconfigureCellsForItems:@[ _tabPickupDetailItem ]];
}

@end

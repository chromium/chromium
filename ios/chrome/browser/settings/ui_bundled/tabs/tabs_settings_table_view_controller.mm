// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_table_view_controller_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInactiveTabs = kSectionIdentifierEnumZero,
  SectionIdentifierTabGroups,
};

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeInactiveTabs = kItemTypeEnumZero,
  ItemTypeAutomaticallyOpenTabGroups,
};

}  // namespace

@implementation TabsSettingsTableViewController {
  // Updatable inactive tabs item.
  TableViewDetailIconItem* _inactiveTabsDetailItem;
  // Switch item for automatically open tab groups from other devices.
  TableViewSwitchItem* _automaticallyOpenTabGroupsItem;
  // Current inactive tab days threshold.
  int _inactiveDaysThreshold;
  // Whether current automatically open tab groups enabled.
  BOOL _automaticallyOpenTabGroupsEnabled;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(
        IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled()
            ? IDS_IOS_TABS_AND_TAB_GROUPS_MANAGEMENT_SETTINGS
            : IDS_IOS_TABS_MANAGEMENT_SETTINGS);
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

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.dismissalDelegate tabsSettingsTableViewControllerDidDisappear:self];
  }
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierInactiveTabs];
  [model addItem:[self moveInactiveTabsItem]
      toSectionWithIdentifier:SectionIdentifierInactiveTabs];
  [self updateInactiveTabsItemWithDaysThreshold:_inactiveDaysThreshold];

  if (IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierTabGroups];
    [model addItem:[self automaticallyOpenTabGroupsItem]
        toSectionWithIdentifier:SectionIdentifierTabGroups];
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

- (void)setAutomaticallyOpenTabGroupsEnabled:(BOOL)enabled {
  CHECK(IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled());
  _automaticallyOpenTabGroupsEnabled = enabled;
  // Do not update UI when model is not loaded.
  if (!_automaticallyOpenTabGroupsItem) {
    return;
  }
  if (_automaticallyOpenTabGroupsItem.on == enabled) {
    return;
  }
  _automaticallyOpenTabGroupsItem.on = enabled;
  [self reconfigureCellsForItems:@[ _automaticallyOpenTabGroupsItem ]];
}

#pragma mark - Model Items

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

// Returns a newly created TableViewSwitchItem for the automatically open tab
// groups settings menu.
- (TableViewSwitchItem*)automaticallyOpenTabGroupsItem {
  CHECK(IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled());
  _automaticallyOpenTabGroupsItem = [[TableViewSwitchItem alloc]
      initWithType:ItemTypeAutomaticallyOpenTabGroups];
  _automaticallyOpenTabGroupsItem.text = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_AUTOMATICALLY_OPEN_SYNCED_TAB_GROUPS_TITLE);
  _automaticallyOpenTabGroupsItem.on = _automaticallyOpenTabGroupsEnabled;
  _automaticallyOpenTabGroupsItem.accessibilityIdentifier =
      kSettingsAutomaticallyOpenTabGroupsCellId;
  _automaticallyOpenTabGroupsItem.target = self;
  _automaticallyOpenTabGroupsItem.selector =
      @selector(openTabGroupsSwitchToggled:);
  return _automaticallyOpenTabGroupsItem;
}

#pragma mark - Switch Action

- (void)openTabGroupsSwitchToggled:(UISwitch*)sender {
  CHECK(IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled());
  [self.delegate tabsSettingsTableViewController:self
                      didUpdateAutoOpenTabGroups:sender.isOn];
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
    case ItemTypeAutomaticallyOpenTabGroups:
      break;
  }
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

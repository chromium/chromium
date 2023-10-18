// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_commands.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Sections identifier.
enum SectionIdentifier {
  kOptions = kSectionIdentifierEnumZero,
};

// Item types to enumerate the table items.
enum ItemType {
  kSwitch = kItemTypeEnumZero,
  kFeatureInformation,
};

}  // namespace.

@interface TabPickupSettingsTableViewController ()

// Switch item that tracks the state of the tab pickup feature.
@property(nonatomic, strong) TableViewSwitchItem* tabPickupSwitchItem;

@end

@implementation TabPickupSettingsTableViewController {
  // State of the tab pickup feature.
  BOOL _tabPickupEnabled;
  // State of the tab sync feature.
  BOOL _tabSyncEnabled;
}

#pragma mark - Initialization

- (instancetype)init {
  CHECK(IsTabPickupEnabled());
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP_SCREEN_TITLE);
    _tabSyncEnabled = YES;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kTabPickupSettingsTableViewId;
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifier::kOptions];

  [model addItem:self.tabPickupSwitchItem
      toSectionWithIdentifier:SectionIdentifier::kOptions];
  [model setFooter:[self featureInformationItem]
      forSectionWithIdentifier:SectionIdentifier::kOptions];
}

#pragma mark - Public

- (void)reloadSwitchItem {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemType::kSwitch
                              sectionIdentifier:SectionIdentifier::kOptions];
  TableViewSwitchCell* switchCell =
      base::apple::ObjCCastStrict<TableViewSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  BOOL on = _tabSyncEnabled && _tabPickupEnabled;
  UISwitch* switchView = switchCell.switchView;

  // Update the switch cell.
  if (on != switchView.isOn) {
    [switchView setOn:on animated:YES];
  }

  // Also update the switch item.
  TableViewSwitchItem* tabPickupSwitchItem = self.tabPickupSwitchItem;
  tabPickupSwitchItem.on = on;
  [self reconfigureCellsForItems:@[ tabPickupSwitchItem ]];
}

#pragma mark - TabPickupSettingsConsumer

- (void)setTabPickupEnabled:(BOOL)enabled {
  _tabPickupEnabled = enabled;
  TableViewSwitchItem* tabPickupSwitchItem = self.tabPickupSwitchItem;
  if (tabPickupSwitchItem.on == enabled) {
    return;
  }
  [self reloadSwitchItem];
}

- (void)setTabSyncEnabled:(BOOL)enabled {
  if (_tabSyncEnabled == enabled) {
    return;
  }
  _tabSyncEnabled = enabled;
  [self reloadSwitchItem];
}

#pragma mark - Properties

- (TableViewSwitchItem*)tabPickupSwitchItem {
  if (!_tabPickupSwitchItem) {
    _tabPickupSwitchItem =
        [[TableViewSwitchItem alloc] initWithType:ItemType::kSwitch];
    _tabPickupSwitchItem.text =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP);
    _tabPickupSwitchItem.accessibilityIdentifier =
        kTabPickupSettingsSwitchItemId;
  }
  return _tabPickupSwitchItem;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemType::kSwitch) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* headerView = [super tableView:tableView
                 viewForHeaderInSection:section];
  TableViewLinkHeaderFooterView* header =
      base::apple::ObjCCast<TableViewLinkHeaderFooterView>(headerView);
  if (header) {
    header.delegate = self;
  }
  return headerView;
}

#pragma mark - Private

// Updates the switch item value and informs the model.
- (void)switchChanged:(UISwitch*)switchView {
  self.tabPickupSwitchItem.on = switchView.isOn;
  if (switchView.isOn && !_tabSyncEnabled) {
    // The switch is set to ON, but the ivars are not updated. If the user
    // cancels the sign-in flow, the switch should be reloaded to its initial
    // state.
    [self.tabPickupSettingsHandler showSign];
    return;
  }

  [self.delegate tabPickupSettingsTableViewController:self
                                   didEnableTabPickup:switchView.isOn];
}

// Creates a TableViewHeaderFooterItem that shows informations about tab
// pickup.
- (TableViewHeaderFooterItem*)featureInformationItem {
  TableViewLinkHeaderFooterItem* featureInformationItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemType::kFeatureInformation];
  featureInformationItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP_SCREEN_FOOTER);
  return featureInformationItem;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileTabPickupSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileTabPickupSettingsBack"));
}

@end

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
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Sections identifier.
enum SectionIdentifier {
  kOptions = kSectionIdentifierEnumZero,
  kPrivacy,
};

// Item types to enumerate the table items.
enum ItemType {
  kSwitch = kItemTypeEnumZero,
  kFeatureInformation,
  kPrivacyInformation,
};

// Used to open the Sync and Google services settings.
// These links should not be dispatched.
const char kGoogleServicesSettingsURL[] = "settings://open_google_services";
const char kSyncSettingsURL[] = "settings://open_sync";

}  // namespace.

@interface TabPickupSettingsTableViewController ()

// Switch item that tracks the state of the tab pickup feature.
@property(nonatomic, strong) TableViewSwitchItem* tabPickupSwitchItem;

// Footer that shows a link to open the Sync and Google Services settings.
@property(nonatomic, strong)
    TableViewLinkHeaderFooterItem* privacyInformationItem;

@end

@implementation TabPickupSettingsTableViewController {
  // State of the tab pickup feature.
  bool _tabPickupEnabled;
  // State of the tab sync feature.
  bool _tabSyncEnabled;
}

#pragma mark - Initialization

- (instancetype)init {
  CHECK(IsTabPickupEnabled());
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP_SCREEN_TITLE);
    _tabSyncEnabled = true;
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
  [self updatePrivacyInformationItem];

  [model addSectionWithIdentifier:SectionIdentifier::kOptions];
  [model addSectionWithIdentifier:SectionIdentifier::kPrivacy];

  [model addItem:self.tabPickupSwitchItem
      toSectionWithIdentifier:SectionIdentifier::kOptions];
  [model setFooter:[self featureInformationItem]
      forSectionWithIdentifier:SectionIdentifier::kOptions];
  [model setHeader:self.privacyInformationItem
      forSectionWithIdentifier:SectionIdentifier::kPrivacy];
}

#pragma mark - TabPickupSettingsConsumer

- (void)setTabPickupEnabled:(bool)enabled {
  _tabPickupEnabled = enabled;
  TableViewSwitchItem* tabPickupSwitchItem = self.tabPickupSwitchItem;
  if (tabPickupSwitchItem.on == enabled) {
    return;
  }
  tabPickupSwitchItem.on = _tabSyncEnabled && enabled;
  [self reconfigureCellsForItems:@[ tabPickupSwitchItem ]];
}

- (void)setTabSyncEnabled:(bool)enabled {
  if (_tabSyncEnabled == enabled) {
    return;
  }
  _tabSyncEnabled = enabled;
  TableViewSwitchItem* tabPickupSwitchItem = self.tabPickupSwitchItem;
  tabPickupSwitchItem.enabled = enabled;
  tabPickupSwitchItem.on = _tabPickupEnabled && enabled;
  [self reconfigureCellsForItems:@[ tabPickupSwitchItem ]];

  [self updatePrivacyInformationItem];
  [self reconfigureCellsForItems:@[ self.privacyInformationItem ]];
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

- (TableViewHeaderFooterItem*)privacyInformationItem {
  if (!_privacyInformationItem) {
    _privacyInformationItem = [[TableViewLinkHeaderFooterItem alloc]
        initWithType:ItemType::kPrivacyInformation];
  }
  return _privacyInformationItem;
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

// Updates the privacy information item according to the current state of the
// sync feature.
- (void)updatePrivacyInformationItem {
  NSString* privacyFooterText;
  NSMutableArray* URLs = [[NSMutableArray alloc] init];
  if (_tabSyncEnabled) {
    privacyFooterText =
        l10n_util::GetNSString(IDS_IOS_PRIVACY_SYNC_AND_GOOGLE_SERVICES_FOOTER);
    [URLs addObject:[[CrURL alloc] initWithGURL:GURL(kSyncSettingsURL)]];
  } else {
    privacyFooterText =
        l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER);
  }
  [URLs
      addObject:[[CrURL alloc] initWithGURL:GURL(kGoogleServicesSettingsURL)]];

  TableViewLinkHeaderFooterItem* privacyInformationItem =
      self.privacyInformationItem;
  privacyInformationItem.accessibilityIdentifier =
      kTabPickupSettingsPrivacyFooterId;
  privacyInformationItem.text = privacyFooterText;
  privacyInformationItem.urls = URLs;
}

// Updates the switch item value and informs the model.
- (void)switchChanged:(UISwitch*)switchView {
  self.tabPickupSwitchItem.on = switchView.isOn;
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

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  if (URL.gurl == GURL(kGoogleServicesSettingsURL)) {
    [self.dispatcher showGoogleServicesSettingsFromViewController:self];
  } else if (URL.gurl == GURL(kSyncSettingsURL)) {
    [self.dispatcher showSyncSettingsFromViewController:self];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileTabPickupSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileTabPickupSettingsBack"));
}

@end

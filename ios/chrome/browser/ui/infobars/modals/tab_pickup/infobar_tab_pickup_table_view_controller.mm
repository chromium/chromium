// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/tab_pickup/infobar_tab_pickup_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/tab_pickup/infobar_tab_pickup_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/tab_pickup/infobar_tab_pickup_table_view_controller_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Top spacing between the navigation bar and the first item.
const CGFloat kTableViewTopSpacing = 16;

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

@interface InfobarTabPickupTableViewController ()

// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Switch item that tracks the state of the tab pickup feature.
@property(nonatomic, strong) TableViewSwitchItem* tabPickupSwitchItem;

@end

@implementation InfobarTabPickupTableViewController {
  // State of the tab pickup feature.
  BOOL _tabPickupEnabled;
  // State of the tab sync feature.
  BOOL _tabSyncEnabled;
}

- (instancetype)init {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeTabPickup];
    self.title =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP_SCREEN_TITLE);
    _tabSyncEnabled = YES;
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kTabPickupModalTableViewId;
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, kTableViewTopSpacing)];

  // Configure the NavigationBar.
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissInfobarModal)];
  doneButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.rightBarButtonItem = doneButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifier::kOptions];

  [model addItem:self.tabPickupSwitchItem
      toSectionWithIdentifier:SectionIdentifier::kOptions];
  [model addItem:[self featureInformationItem]
      toSectionWithIdentifier:SectionIdentifier::kOptions];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.delegate modalInfobarWasDismissed:self];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - InfobarTabPickupConsumer

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
}

#pragma mark - Properties

- (TableViewSwitchItem*)tabPickupSwitchItem {
  if (!_tabPickupSwitchItem) {
    _tabPickupSwitchItem =
        [[TableViewSwitchItem alloc] initWithType:ItemType::kSwitch];
    _tabPickupSwitchItem.text =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP);
    _tabPickupSwitchItem.accessibilityIdentifier = kTabPickupModalSwitchItemId;
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
  switch (itemType) {
    case ItemType::kSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(switchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemType::kFeatureInformation: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
  }
  return cell;
}

#pragma mark - Private

// Updates the switch item value and informs the model.
- (void)switchChanged:(UISwitch*)switchView {
  self.tabPickupSwitchItem.on = switchView.isOn;
  [self.delegate infobarTabPickupTableViewController:self
                                  didEnableTabPickup:switchView.isOn];
}

// Creates a TableViewHeaderFooterItem that shows informations about tab
// pickup.
- (TableViewTextItem*)featureInformationItem {
  TableViewTextItem* featureInformationItem =
      [[TableViewTextItem alloc] initWithType:ItemType::kFeatureInformation];
  featureInformationItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_TAB_PICKUP_SCREEN_FOOTER);
  featureInformationItem.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  featureInformationItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return featureInformationItem;
}

#pragma mark - Private Methods

// Dismisses the infobar modal.
- (void)dismissInfobarModal {
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.delegate dismissInfobarModal:self];
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_table_view_controller_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierWebInspectorEnabled = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSettingsWebInspectorEnabled = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface WebInspectorStateTableViewController ()

// The item related to the switch for the "Web Inspector" setting.
@property(nonatomic, strong) TableViewSwitchItem* webInspectorEnabledItem;

@end

@implementation WebInspectorStateTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_WEB_INSPECTOR_TITLE);
  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierWebInspectorEnabled];
  [model addItem:self.webInspectorEnabledItem
      toSectionWithIdentifier:SectionIdentifierWebInspectorEnabled];

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
          ? l10n_util::GetNSString(IDS_IOS_WEB_INSPECTOR_SUBTITLE_IPAD)
          : l10n_util::GetNSString(IDS_IOS_WEB_INSPECTOR_SUBTITLE_IPHONE);
  [model setFooter:footer
      forSectionWithIdentifier:SectionIdentifierWebInspectorEnabled];
}

#pragma mark - WebInspectorStateConsumer

- (void)setWebInspectorEnabled:(BOOL)enabled {
  self.webInspectorEnabledItem.on = enabled;
  [self reconfigureCellsForItems:@[ self.webInspectorEnabledItem ]];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileWebInspectorSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileWebInspectorSettingsBack"));
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeSettingsWebInspectorEnabled: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView
                 addTarget:self
                    action:@selector(webInspectorEnabledSwitchChanged:)
          forControlEvents:UIControlEventValueChanged];
      break;
    }
  }
  return cell;
}

#pragma mark - Actions

- (void)webInspectorEnabledSwitchChanged:(UISwitch*)switchView {
  [self.delegate didEnableWebInspector:switchView.on];
}

#pragma mark - Properties
- (TableViewSwitchItem*)webInspectorEnabledItem {
  if (!_webInspectorEnabledItem) {
    _webInspectorEnabledItem = [[TableViewSwitchItem alloc]
        initWithType:ItemTypeSettingsWebInspectorEnabled];

    _webInspectorEnabledItem.text =
        l10n_util::GetNSString(IDS_IOS_WEB_INSPECTOR_LABEL);
  }

  return _webInspectorEnabledItem;
}

@end

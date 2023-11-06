// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTrackingPriceContent = kSectionIdentifierEnumZero,
  SectionIdentifierTrackingPriceEmailNotifications,
};

}  // namespace

@interface TrackingPriceViewController ()

// Mobile notification table view item received by mediator.
@property(nonatomic, strong) TableViewItem* mobileNotificationItem;
// Tracking price header received by mediator.
@property(nonatomic, strong) TableViewHeaderFooterItem* trackPriceHeaderItem;
// Email notification table view item received by mediator.
@property(nonatomic, strong) TableViewItem* emailNotificationItem;

@end

@implementation TrackingPriceViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACKING_TITLE);
  self.tableView.accessibilityIdentifier = kTrackingPriceTableViewId;
  [self loadModel];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileTrackingPriceSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileTrackingPriceSettingsBack"));
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierTrackingPriceContent];
  [model addSectionWithIdentifier:
             SectionIdentifierTrackingPriceEmailNotifications];
  [model addItem:self.mobileNotificationItem
      toSectionWithIdentifier:SectionIdentifierTrackingPriceContent];
  [model addItem:self.emailNotificationItem
      toSectionWithIdentifier:SectionIdentifierTrackingPriceEmailNotifications];
  [model setHeader:self.trackPriceHeaderItem
      forSectionWithIdentifier:SectionIdentifierTrackingPriceContent];
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate trackingPriceViewControllerDidRemove:self];
  }
}

#pragma mark - Private

// Called when switch is toggled.
- (void)switchAction:(UISwitch*)sender {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:sender.tag];
  DCHECK(indexPath);
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.modelDelegate toggleSwitchItem:item withValue:sender.isOn];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[TableViewSwitchCell class]]) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    switchCell.switchView.tag = item.type;
  }
  return cell;
}

@end

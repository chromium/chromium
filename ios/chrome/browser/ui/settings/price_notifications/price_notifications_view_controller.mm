// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_constants.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPriceNotificationsContent = kSectionIdentifierEnumZero,
};

}  // namespace

@interface PriceNotificationsViewController ()

// All the items for the price notifications section received by mediator.
@property(nonatomic, strong) TableViewItem* priceTrackingItem;

@end

@implementation PriceNotificationsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_TITLE);
  self.tableView.accessibilityIdentifier = kPriceNotificationsTableViewId;
  [self loadModel];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePriceNotificationsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePriceNotificationsSettingsBack"));
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPriceNotificationsContent];
  [model addItem:self.priceTrackingItem
      toSectionWithIdentifier:SectionIdentifierPriceNotificationsContent];
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate priceNotificationsViewControllerDidRemove:self];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];
  [self.modelDelegate didSelectItem:selectedItem];
}

@end

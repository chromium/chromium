// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/notifications/content_notifications/content_notifications_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/content_notifications/content_notifications_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContentNotifications = kSectionIdentifierEnumZero,
};

}  // namespace

@interface ContentNotificationsViewController ()

// All the items for the content notifications section received by mediator.
@property(nonatomic, strong) TableViewSwitchItem* contentNotificationsItem;
// All the items for the Sports notifications section received by mediator.
@property(nonatomic, strong) TableViewSwitchItem* sportsNotificationsItem;
// Content Notifications footer item received by the mediator.
@property(nonatomic, strong)
    TableViewHeaderFooterItem* contentNotificationsFooterItem;

@end

@implementation ContentNotificationsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_TITLE);
  self.tableView.accessibilityIdentifier = kContentNotificationsTableViewId;
  [self loadModel];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileContentNotificationsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileContentNotificationsSettingsBack"));
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContentNotifications];
  [model addItem:self.contentNotificationsItem
      toSectionWithIdentifier:SectionIdentifierContentNotifications];
  [model addItem:self.sportsNotificationsItem
      toSectionWithIdentifier:SectionIdentifierContentNotifications];
  [model setFooter:self.contentNotificationsFooterItem
      forSectionWithIdentifier:SectionIdentifierContentNotifications];
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        contentNotificationsViewControllerDidRemove:self];
  }
}

@end

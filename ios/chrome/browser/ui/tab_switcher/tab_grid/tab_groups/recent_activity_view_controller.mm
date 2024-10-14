// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_log_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The size of the close button.
const CGFloat kCloseButtonSize = 28;

typedef NSDiffableDataSourceSnapshot<NSString*, RecentActivityLogItem*>
    ActivityLogSnapshot;
typedef UITableViewDiffableDataSource<NSString*, RecentActivityLogItem*>
    ActivityLogDiffableDataSource;

NSString* const kRecentActivitySectionIdentifier =
    @"RecentActivitySectionIdentifier";

}  // namespace

@implementation RecentActivityViewController {
  ActivityLogDiffableDataSource* _dataSource;
}

#pragma mark - ChromeTableViewController

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  return [super initWithStyle:style];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GROUP_RECENT_ACTIVITY_SHEET_TITLE);

  // Configure a close button.
  UIImage* buttonImage = SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kCloseButtonSize), @[
        [UIColor colorNamed:kCloseButtonColor],
        [UIColor colorNamed:kSecondaryBackgroundColor]
      ]);
  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithImage:buttonImage
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(didTapCloseButton)];
  self.navigationItem.rightBarButtonItem = closeButton;

  // Configure a table view.
  UITableView* tableView = self.tableView;
  tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.allowsSelection = NO;
  tableView.scrollEnabled = YES;

  // Configure table items.
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:tableView
           cellProvider:^UITableViewCell*(
               UITableView* view, NSIndexPath* indexPath,
               RecentActivityLogItem* itemIdentifier) {
             return [weakSelf cellForTableView:view
                                     indexPath:indexPath
                                itemIdentifier:itemIdentifier];
           }];

  RegisterTableViewCell<TableViewDetailIconCell>(tableView);
}

#pragma mark - RecentActivityConsumer

- (void)populateItems:(NSArray<RecentActivityLogItem*>*)items {
  CHECK(_dataSource);

  ActivityLogSnapshot* snapshot = [[ActivityLogSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ kRecentActivitySectionIdentifier ]];

  // TODO(crbug.com/370897655): append cells to the data source based on the
  // collected data from MessagingBackendService.

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - Private

// This method is a callback for the right bar button, a close button.
- (void)didTapCloseButton {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

// Configures and returns a cell.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(RecentActivityLogItem*)itemIdentifier {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(tableView);

  // TODO(crbug.com/370897655): Configure a cell depending on the type of the
  // cell.
  switch (itemIdentifier.type) {
    case ActivityLogType::kTabAdded:
      break;
    case ActivityLogType::kTabRemoved:
      break;
    case ActivityLogType::kTabNavigated:
      break;
    case ActivityLogType::kUserLeft:
      break;
    case ActivityLogType::kGroupColorChanged:
      break;
    case ActivityLogType::kGroupNameChanged:
      break;
  }

  return cell;
}

@end

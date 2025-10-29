// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_view_controller.h"

#import "base/check.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/share_kit/model/share_kit_avatar_primitive.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_log_cell.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_log_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kButtonImageSize = 18;

typedef NSDiffableDataSourceSnapshot<NSString*, RecentActivityLogItem*>
    ActivityLogSnapshot;
typedef UITableViewDiffableDataSource<NSString*, RecentActivityLogItem*>
    ActivityLogDiffableDataSource;

NSString* const kRecentActivitySectionIdentifier =
    @"RecentActivitySectionIdentifier";

// Returns the accessibility identifier to set on a RecentActivityLogCell when
// positioned at the given index.
NSString* RecentActivityLogCellAccessibilityIdentifier(NSUInteger index) {
  return [NSString
      stringWithFormat:@"%@%ld", kRecentActivityLogCellIdentifierPrefix, index];
}

}  // namespace

@interface RecentActivityViewController ()
@property(nonatomic, strong) ActivityLogDiffableDataSource* dataSource;
@end

@implementation RecentActivityViewController

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

  self.view.accessibilityIdentifier = kTabGroupRecentActivityIdentifier;

  __weak __typeof(self) weakSelf = self;

  UIAction* showAllActivity =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_SHARE_KIT_MANAGE_ACTIVITY_LOG_TITLE)
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf showFullActivity];
                        }];
  UIMenu* menu = [UIMenu menuWithChildren:@[ showAllActivity ]];

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(didTapCloseButton)];
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kRecentActivityLogCloseButtonIdentifier;

  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithImage:DefaultSymbolWithPointSize(kMenuSymbol, kButtonImageSize)
               menu:menu];
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kRecentActivityLogMenuButtonIdentifier;

  // Configure a table view.
  UITableView* tableView = self.tableView;
  tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.scrollEnabled = YES;

  RegisterTableViewCell<RecentActivityLogCell>(tableView);
}

#pragma mark - RecentActivityConsumer

- (void)populateItems:(NSArray<RecentActivityLogItem*>*)items {
  BOOL empty = items.count == 0;
  self.tableView.backgroundView = empty ? [self emptyStateLabel] : nil;
  if (empty) {
    return;
  }

  ActivityLogSnapshot* snapshot = [[ActivityLogSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ kRecentActivitySectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items];

  [self.dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.mutator
      performActionForItem:[_dataSource itemIdentifierForIndexPath:indexPath]];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Private

// Returns an empty state label.
- (UILabel*)emptyStateLabel {
  UILabel* emptyStateLabel = [[UILabel alloc] init];
  emptyStateLabel.text = l10n_util::GetNSString(
      IDS_IOS_TAB_GROUP_RECENT_ACTIVITY_SHEET_EMPTY_MESSAGE);
  emptyStateLabel.textAlignment = NSTextAlignmentCenter;
  emptyStateLabel.numberOfLines = 0;
  emptyStateLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  emptyStateLabel.adjustsFontForContentSizeCategory = YES;
  emptyStateLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  return emptyStateLabel;
}

// Overrides the getter to the data store to allow the lazy initialization.
- (ActivityLogDiffableDataSource*)dataSource {
  if (!_dataSource) {
    // Lazy initialization of the data source.
    __weak __typeof(self) weakSelf = self;
    _dataSource = [[UITableViewDiffableDataSource alloc]
        initWithTableView:self.tableView
             cellProvider:^UITableViewCell*(
                 UITableView* view, NSIndexPath* indexPath,
                 RecentActivityLogItem* itemIdentifier) {
               return [weakSelf cellForTableView:view
                                       indexPath:indexPath
                                  itemIdentifier:itemIdentifier];
             }];
  }
  return _dataSource;
}

// This method is a callback for the right bar button, a close button.
- (void)didTapCloseButton {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

// This is called when the user wants to see the activity log.
- (void)showFullActivity {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
  GURL activityLogsURL = GURL(data_sharing::features::kActivityLogsURL.Get());
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:activityLogsURL];
  [self.applicationHandler closePresentedViewsAndOpenURL:command];
}

// Configures and returns a cell.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(RecentActivityLogItem*)itemIdentifier {
  RecentActivityLogCell* cell =
      DequeueTableViewCell<RecentActivityLogCell>(tableView);
  cell.titleLabel.text = itemIdentifier.title;
  NSString* descriptionString =
      [NSString stringWithFormat:@"%@ • %@", itemIdentifier.actionDescription,
                                 itemIdentifier.elapsedTime];
  cell.descriptionLabel.text = descriptionString;
  cell.accessibilityIdentifier =
      RecentActivityLogCellAccessibilityIdentifier(indexPath.item);
  [cell.faviconView configureWithAttributes:itemIdentifier.attributes];

  NSString* uniqueIdentifier = cell.uniqueIdentifier;
  CrURL* crurl = [[CrURL alloc] initWithGURL:itemIdentifier.faviconURL];
  [_faviconDataSource
      faviconForPageURL:crurl
             completion:^(FaviconAttributes* attributes, bool cached) {
               CHECK(attributes);
               // Only set favicon if the cell hasn't been reused.
               if ([cell.uniqueIdentifier isEqualToString:uniqueIdentifier]) {
                 [cell.faviconView configureWithAttributes:attributes];
               }
             }];

  [cell setAvatar:[itemIdentifier.avatarPrimitive view]];
  [itemIdentifier.avatarPrimitive resolve];
  return cell;
}

@end

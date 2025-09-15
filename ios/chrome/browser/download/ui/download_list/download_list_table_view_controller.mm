// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_group_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_grouping_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_illustrated_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// Diffable data source types using DownloadListGroupItem for section
// identifiers.
typedef UITableViewDiffableDataSource<DownloadListGroupItem*, DownloadListItem*>
    DownloadListDiffableDataSource;
typedef NSDiffableDataSourceSnapshot<DownloadListGroupItem*, DownloadListItem*>
    DownloadListSnapshot;

@implementation DownloadListTableViewController {
  DownloadListDiffableDataSource* _diffableDataSource;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_LIST_TITLE);

  // Configure navigation bar.
  self.navigationController.navigationBar.prefersLargeTitles = YES;
  UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(closeButtonTapped)];
  self.navigationItem.rightBarButtonItem = closeButton;

  // Configure table view.
  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);
  [self configureDiffableDataSource];

  // Load download records.
  [self.mutator loadDownloadRecords];
}

#pragma mark - Private

/// Dismisses the view controller when close button is tapped.
- (void)closeButtonTapped {
  [self.downloadListHandler hideDownloadList];
}

/// Configures the diffable data source for the table view.
- (void)configureDiffableDataSource {
  __weak __typeof(self) weakSelf = self;
  _diffableDataSource = [[DownloadListDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          DownloadListItem* item) {
             return [weakSelf cellForItem:item atIndexPath:indexPath];
           }];
  self.tableView.dataSource = _diffableDataSource;
}

/// Creates and configures a cell for the given item at the specified index
/// path.
- (UITableViewCell*)cellForItem:(DownloadListItem*)item
                    atIndexPath:(NSIndexPath*)indexPath {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self.tableView);
  cell.textLabel.text = item.fileName;
  NSString* detailText = item.detailText;
  [cell setDetailText:detailText];
  if (detailText.length > 0) {
    cell.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  }
  [cell setIconImage:item.fileTypeIcon
            tintColor:nil
      backgroundColor:nil
         cornerRadius:0];
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/440222083): Implement download primary action handling.
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  // Get the section identifier (DownloadListGroupItem) from the data source
  // snapshot.
  DownloadListGroupItem* groupItem =
      [_diffableDataSource sectionIdentifierForIndex:section];

  TableViewTextHeaderFooterView* headerView =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(tableView);
  [headerView setTitle:groupItem.title];

  return headerView;
}

#pragma mark - DownloadListConsumer

- (void)setDownloadListItems:(NSArray<DownloadListItem*>*)items {
  // Create a new snapshot with date-grouped sections.
  DownloadListSnapshot* snapshot = [[DownloadListSnapshot alloc] init];

  // Group items by date.
  NSArray<DownloadListGroupItem*>* groupItems =
      download_list_grouping_util::GroupDownloadItemsByDate(items);

  // Add sections and items.
  for (DownloadListGroupItem* groupItem in groupItems) {
    [snapshot appendSectionsWithIdentifiers:@[ groupItem ]];
    [snapshot appendItemsWithIdentifiers:groupItem.items
               intoSectionWithIdentifier:groupItem];
  }

  // Detect changes between old and new items.
  NSArray<DownloadListItem*>* oldItems =
      _diffableDataSource.snapshot.itemIdentifiers;
  if (oldItems.count > 0) {
    NSSet<DownloadListItem*>* oldItemsSet = [NSSet setWithArray:oldItems];
    NSMutableArray<DownloadListItem*>* changedItems =
        [[NSMutableArray alloc] init];

    for (DownloadListItem* newItem in items) {
      DownloadListItem* existingItem = [oldItemsSet member:newItem];
      if (existingItem && ![existingItem isEqualToItem:newItem]) {
        [changedItems addObject:newItem];
      }
    }

    if (changedItems.count > 0) {
      [snapshot reconfigureItemsWithIdentifiers:changedItems];
    }
  }

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (void)setLoadingState:(BOOL)loading {
}

- (void)setEmptyState:(BOOL)empty {
  if (empty) {
    // Empty downloads: show small title and empty view.
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    if (!self.tableView.backgroundView) {
      UIImage* emptyImage = [UIImage imageNamed:@"download_list_empty"];
      TableViewIllustratedEmptyView* emptyView =
          [[TableViewIllustratedEmptyView alloc]
              initWithFrame:self.view.bounds
                      image:emptyImage
                      title:l10n_util::GetNSString(
                                IDS_IOS_DOWNLOAD_LIST_NO_ENTRIES_TITLE)
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_DOWNLOAD_LIST_NO_ENTRIES_MESSAGE)];
      self.tableView.backgroundView = emptyView;
    }
  } else {
    // Non-empty downloads: show large title initially and hide empty view.
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeAlways;
    self.tableView.backgroundView = nil;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

/// Called before the presentation controller will dismiss.
- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  [self.downloadListHandler hideDownloadList];
}

@end

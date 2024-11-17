// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_activity_summary_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_header.h"

@interface TabGroupGridViewController () <TabGroupActivitySummaryCellDelegate>
@end

@implementation TabGroupGridViewController {
  // Registered header.
  UICollectionViewSupplementaryRegistration* _tabGroupHeaderRegistration;
  // The cell registration for the summary card of the recent activity in a
  // shared tab group.
  UICollectionViewCellRegistration* _activitySummaryCellRegistration;
  // Whether this tab group is shared with other users.
  BOOL _shared;
}

- (instancetype)initWithShared:(BOOL)shared {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    _shared = shared;
  }
  return self;
}

- (void)setGroupColor:(UIColor*)groupColor {
  if ([_groupColor isEqual:groupColor]) {
    return;
  }
  _groupColor = groupColor;
  [self updateTabGroupHeader];
}

- (void)setGroupTitle:(NSString*)groupTitle {
  if ([_groupTitle isEqual:groupTitle]) {
    return;
  }
  _groupTitle = groupTitle;
  [self updateTabGroupHeader];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  CGFloat headerHeight = [self header].bounds.size.height;
  BOOL headerHidden = headerHeight < scrollView.contentOffset.y;
  [self.viewDelegate gridViewHeaderHidden:headerHidden];
  [super scrollViewDidScroll:scrollView];
}

#pragma mark - Parent's functions

// Returns a configured header for the given index path.
- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  return [self.collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:
          _tabGroupHeaderRegistration
                                                    forIndexPath:indexPath];
}

- (UICollectionViewCell*)cellForItemAtIndexPath:(NSIndexPath*)indexPath
                                 itemIdentifier:
                                     (GridItemIdentifier*)itemIdentifier {
  if (itemIdentifier.type == GridItemType::kActivitySummary) {
    return [self.collectionView
        dequeueConfiguredReusableCellWithRegistration:
            _activitySummaryCellRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }

  return [super cellForItemAtIndexPath:indexPath itemIdentifier:itemIdentifier];
}

- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;
  // Register TabGroupHeader.
  auto configureTabGroupHeader =
      ^(TabGroupHeader* header, NSString* elementKind, NSIndexPath* indexPath) {
        [weakSelf configureTabGroupHeader:header];
      };
  _tabGroupHeaderRegistration = [UICollectionViewSupplementaryRegistration
      registrationWithSupplementaryClass:[TabGroupHeader class]
                             elementKind:UICollectionElementKindSectionHeader
                    configurationHandler:configureTabGroupHeader];

  // Register ActivitySummaryCell.
  auto configureActivitySummaryCell =
      ^(TabGroupActivitySummaryCell* cell, NSIndexPath* indexPath, id item) {
        [weakSelf configureActivitySummaryCell:cell];
      };
  _activitySummaryCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:TabGroupActivitySummaryCell.class
           configurationHandler:configureActivitySummaryCell];

  [super createRegistrations];
}

- (MenuScenarioHistogram)scenarioForContextMenu {
  return kMenuScenarioHistogramTabGroupViewTabEntry;
}

- (void)addAdditionalItemsToSnapshot:(GridSnapshot*)snapshot {
  [self addTabGroupHeaderSectionInSnapshot:snapshot];
  [self addActivitySummaryCellInSnapshot:snapshot];
}

#pragma mark - Private

// Configures the tab group header according to the current state.
- (void)configureTabGroupHeader:(TabGroupHeader*)header {
  header.title = self.groupTitle;
  header.color = self.groupColor;
}

// Configures the activity summary cell for a shared tab group.
- (void)configureActivitySummaryCell:(TabGroupActivitySummaryCell*)cell {
  // TODO(crbug.com/375594684): Connect with the backend and update the text.
  cell.text = @"3 new tabs, 2 closed";
  cell.delegate = self;
}

// Returns the header which contains the title and the color view.
- (TabGroupHeader*)header {
  NSInteger headerSectionIndex = [self.diffableDataSource
      indexForSectionIdentifier:kTabGroupHeaderSectionIdentifier];
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0
                                               inSection:headerSectionIndex];
  TabGroupHeader* header =
      base::apple::ObjCCast<TabGroupHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  return header;
}

// Updates the tab group header with the current state.
- (void)updateTabGroupHeader {
  [self configureTabGroupHeader:[self header]];
}

// Adds the tab group header section if it doens't exist in the snapshot yet.
- (void)addTabGroupHeaderSectionInSnapshot:(GridSnapshot*)snapshot {
  NSInteger sectionIndex =
      [snapshot indexOfSectionIdentifier:kTabGroupHeaderSectionIdentifier];

  if (sectionIndex == NSNotFound) {
    [snapshot
        insertSectionsWithIdentifiers:@[ kTabGroupHeaderSectionIdentifier ]
          beforeSectionWithIdentifier:kGridOpenTabsSectionIdentifier];
  }
}

// Adds the activity summary cell if it doesn't exist in the snapshot yet.
// Reconfigures the cell if it already exists.
- (void)addActivitySummaryCellInSnapshot:(GridSnapshot*)snapshot {
  if (!_shared) {
    return;
  }

  // TODO(crbug.com/370898260): Add more condition to display the summary. Now,
  // the summary card is always displayed when the group is shared.

  GridItemIdentifier* item = [GridItemIdentifier activitySummaryIdentifier];

  if ([snapshot indexOfItemIdentifier:item] != NSNotFound) {
    [snapshot reconfigureItemsWithIdentifiers:@[ item ]];
    return;
  }

  CHECK([snapshot indexOfSectionIdentifier:kTabGroupHeaderSectionIdentifier] !=
        NSNotFound);
  [snapshot appendItemsWithIdentifiers:@[ item ]
             intoSectionWithIdentifier:kTabGroupHeaderSectionIdentifier];
}

#pragma mark - TabGroupActivitySummaryCellDelegate

- (void)closeButtonForActivitySummaryTapped {
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  GridItemIdentifier* item = [GridItemIdentifier activitySummaryIdentifier];
  [snapshot deleteItemsWithIdentifiers:@[ item ]];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)activityButtonForActivitySummaryTapped {
  [self.viewDelegate showRecentActivity];
}

@end

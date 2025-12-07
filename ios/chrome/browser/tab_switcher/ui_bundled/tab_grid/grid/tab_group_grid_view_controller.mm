// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_view_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_activity_summary_cell.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_header.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ui/base/device_form_factor.h"

@interface TabGroupGridViewController () <TabGroupActivitySummaryCellDelegate>
@end

@implementation TabGroupGridViewController {
  // Registered header.
  UICollectionViewSupplementaryRegistration* _tabGroupHeaderRegistration;
  // The cell registration for the summary card of the recent activity in a
  // shared tab group.
  UICollectionViewCellRegistration* _activitySummaryCellRegistration;
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

- (void)setShared:(BOOL)shared {
  if (_shared == shared) {
    return;
  }
  _shared = shared;
  [self updateTabGroupHeader];
}

- (void)setActivitySummaryCellText:(NSString*)text {
  if ([_activitySummaryCellText isEqualToString:text]) {
    return;
  }
  _activitySummaryCellText = [text copy];

  if (text) {
    [self addOrUpdateActivitySummaryCell];
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    text);
  } else {
    [self removeActivitySummaryCell];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  CGFloat headerHeight = [self header].bounds.size.height;
  BOOL headerHidden =
      headerHeight < scrollView.contentOffset.y + scrollView.contentInset.top;
  [self.viewDelegate gridViewHeaderHidden:headerHidden];
  [super scrollViewDidScroll:scrollView];
}

#pragma mark - Parent's functions

- (LegacyGridTransitionLayout*)legacyTransitionLayout {
  LegacyGridTransitionLayout* transitionLayout = [super legacyTransitionLayout];
  // When the user is entering the TabGrid from a Tab in a group, the
  // non-selected tabs should not animate otherwise they will be
  // displayed outside of the container.
  transitionLayout = [LegacyGridTransitionLayout
      layoutWithInactiveItems:@[]
                   activeItem:transitionLayout.activeItem
                selectionItem:transitionLayout.selectionItem];
  return transitionLayout;
}

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
  if (self.activitySummaryCellText != nil) {
    [self addActivitySummaryCellInSnapshot:snapshot];
  }
}

- (EmptyThumbnailLayoutType)layoutTypeForContainerSize:(CGSize)containerSize
                                            isGridCell:(BOOL)isGridCell {
  const CGFloat aspectRatio = TabGridItemAspectRatio(containerSize);
  if (aspectRatio < 1 &&
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return EmptyThumbnailLayoutTypeLandscapeLeading;
  }
  return EmptyThumbnailLayoutTypeCenteredPortrait;
}

#pragma mark - Private

// Configures the tab group header according to the current state.
- (void)configureTabGroupHeader:(TabGroupHeader*)header {
  header.title = self.groupTitle;
  header.color = self.groupColor;
}

// Configures the activity summary cell for a shared tab group.
- (void)configureActivitySummaryCell:(TabGroupActivitySummaryCell*)cell {
  cell.text = self.activitySummaryCellText;
  cell.delegate = self;
  cell.accessibilityIdentifier = kActivitySummaryGridCellIdentifier;
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

  GridItemIdentifier* item = [GridItemIdentifier activitySummaryIdentifier];

  CHECK([snapshot indexOfItemIdentifier:item] == NSNotFound);
  CHECK([snapshot indexOfSectionIdentifier:kTabGroupHeaderSectionIdentifier] !=
        NSNotFound);
  [snapshot appendItemsWithIdentifiers:@[ item ]
             intoSectionWithIdentifier:kTabGroupHeaderSectionIdentifier];
}

// Updates the activity summary cell in the current snapshot if it exists. Adds
// the summary cell if it doesn't exist yet.
- (void)addOrUpdateActivitySummaryCell {
  if (!_shared) {
    return;
  }

  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  if (!self.diffableDataSource || !snapshot) {
    return;
  }

  GridItemIdentifier* item = [GridItemIdentifier activitySummaryIdentifier];

  if ([snapshot indexOfItemIdentifier:item] == NSNotFound) {
    [self addActivitySummaryCellInSnapshot:snapshot];
  } else {
    [snapshot reconfigureItemsWithIdentifiers:@[ item ]];
  }

  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Removes the activity summary cell from the current snapshot.
- (void)removeActivitySummaryCell {
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  GridItemIdentifier* item = [GridItemIdentifier activitySummaryIdentifier];
  if ([snapshot indexOfItemIdentifier:item] == NSNotFound) {
    // Do nothing because the activity summary item doesn't exist.
    return;
  }

  [snapshot deleteItemsWithIdentifiers:@[ item ]];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - TabGroupActivitySummaryCellDelegate

- (void)closeButtonForActivitySummaryTapped {
  [self.delegate didTapButtonInActivitySummary:self];
  [self removeActivitySummaryCell];
}

- (void)activityButtonForActivitySummaryTapped {
  [self.delegate didTapButtonInActivitySummary:self];
  [self removeActivitySummaryCell];
  [self.viewDelegate showRecentActivity];
}

@end

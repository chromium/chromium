// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_grid_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_header.h"

@implementation TabGroupGridViewController {
  // Registered header.
  UICollectionViewSupplementaryRegistration* _tabGroupHeaderRegistration;
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

#pragma mark - Parent's functions

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  CGFloat headerHeight = [self header].bounds.size.height;
  BOOL headerHidden = headerHeight < scrollView.contentOffset.y;
  [self.viewDelegate gridViewHeaderHidden:headerHidden];
  [super scrollViewDidScroll:scrollView];
}

// Returns a configured header for the given index path.
- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  return [self.collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:
          _tabGroupHeaderRegistration
                                                    forIndexPath:indexPath];
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
  [super createRegistrations];
}

- (TabsSectionHeaderType)tabsSectionHeaderTypeForMode:(TabGridMode)mode {
  return TabsSectionHeaderType::kTabGroup;
}

- (MenuScenarioHistogram)scenarioForContextMenu {
  return kMenuScenarioHistogramTabGroupViewTabEntry;
}

#pragma mark - Private

// Configures the tab group header according to the current state.
- (void)configureTabGroupHeader:(TabGroupHeader*)header {
  header.title = self.groupTitle;
  header.color = self.groupColor;
}

// Returns the header which contains the title and the color view.
- (TabGroupHeader*)header {
  NSInteger tabSectionIndex = [self.diffableDataSource
      indexForSectionIdentifier:kGridOpenTabsSectionIdentifier];
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0
                                               inSection:tabSectionIndex];
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

@end

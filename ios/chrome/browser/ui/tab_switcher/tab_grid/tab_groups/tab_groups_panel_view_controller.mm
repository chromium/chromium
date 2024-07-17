// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_favicon_grid.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item_data.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mutator.h"

namespace {

// Layout.
const CGFloat kInterItemSpacing = 24;
const CGFloat kInterGroupSpacing = 16;
const CGFloat kEstimatedItemHeight = 80;
// The minimum width to display two columns. Under that value, display only one
// column.
const CGFloat kColumnCountWidthThreshold = 1000;
const CGFloat kVerticalInset = 20;
const CGFloat kLargeVerticalInset = 40;
const CGFloat kHorizontalInset = 16;
// Percentage of the width dedicated to an horizontal inset when there is only
// one centered element.
const CGFloat kHorizontalInsetPercentageWhenLargeAndOneItem = 0.3125;
// Percentage of the width dedicated to an horizontal inset when there are two
// elements.
const CGFloat kHorizontalInsetPercentageWhenLargeAndTwoItems = 0.125;
NSString* const kTabGroupsSection = @"TabGroups";

typedef NSDiffableDataSourceSnapshot<NSString*, TabGroupsPanelItem*>
    TabGroupsPanelSnapshot;

}  // namespace

@interface TabGroupsPanelViewController () <UICollectionViewDelegate>
@end

@implementation TabGroupsPanelViewController {
  UICollectionView* _collectionView;
  UICollectionViewDiffableDataSource<NSString*, TabGroupsPanelItem*>*
      _dataSource;
  TabGridEmptyStateView* _emptyStateView;
  UIViewPropertyAnimator* _emptyStateAnimator;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.view.accessibilityIdentifier = kTabGroupsPanelIdentifier;

  _collectionView =
      [[UICollectionView alloc] initWithFrame:self.view.bounds
                         collectionViewLayout:[self createLayout]];
  _collectionView.backgroundColor = UIColor.clearColor;
  // CollectionView, in contrast to TableView, doesn’t inset the
  // cell content to the safe area guide by default. We will just manage the
  // collectionView contentInset manually to fit in the safe area instead.
  _collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  _collectionView.backgroundView = [[UIView alloc] init];
  _collectionView.backgroundColor = UIColor.clearColor;
  _collectionView.delegate = self;
  _collectionView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:_collectionView];

  __weak __typeof(self) weakSelf = self;
  UICollectionViewCellRegistration* registration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[TabGroupsPanelCell class]
               configurationHandler:^(TabGroupsPanelCell* cell,
                                      NSIndexPath* indexPath,
                                      TabGroupsPanelItem* item) {
                 [weakSelf configureCell:cell withItem:item];
               }];

  _dataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^(UICollectionView* collectionView,
                               NSIndexPath* indexPath,
                               TabGroupsPanelItem* item) {
                  return [collectionView
                      dequeueConfiguredReusableCellWithRegistration:registration
                                                       forIndexPath:indexPath
                                                               item:item];
                }];

  _emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageTabGroups];
  _emptyStateView.scrollViewContentInsets =
      _collectionView.adjustedContentInset;
  _emptyStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [_collectionView.backgroundView addSubview:_emptyStateView];
  UILayoutGuide* safeAreaGuide =
      _collectionView.backgroundView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [_collectionView.backgroundView.centerYAnchor
        constraintEqualToAnchor:_emptyStateView.centerYAnchor],
    [safeAreaGuide.leadingAnchor
        constraintEqualToAnchor:_emptyStateView.leadingAnchor],
    [safeAreaGuide.trailingAnchor
        constraintEqualToAnchor:_emptyStateView.trailingAnchor],
    [_emptyStateView.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeAreaGuide.topAnchor],
    [_emptyStateView.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeAreaGuide.bottomAnchor],
  ]];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark Public

- (BOOL)isScrolledToTop {
  return IsScrollViewScrolledToTop(_collectionView);
}

- (BOOL)isScrolledToBottom {
  return IsScrollViewScrolledToBottom(_collectionView);
}

- (void)setContentInsets:(UIEdgeInsets)contentInsets {
  // Set the vertical insets on the collection view…
  _collectionView.contentInset =
      UIEdgeInsetsMake(contentInsets.top, 0, contentInsets.bottom, 0);
  // … and the horizontal insets on the layout sections.
  // This is a workaround, as setting the horizontal insets on the collection
  // view isn't honored by the layout when computing the item sizes (items are
  // too big in landscape iPhones with a notch or Dynamic Island).
  _contentInsets = contentInsets;
  [_collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark TabGroupsPanelConsumer

- (void)populateItems:(NSArray<TabGroupsPanelItem*>*)items {
  const BOOL hadOneItem = [self hasOnlyOneItem];

  // Update the data source.
  CHECK(_dataSource);
  TabGroupsPanelSnapshot* snapshot = [[TabGroupsPanelSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kTabGroupsSection ]];
  [snapshot appendItemsWithIdentifiers:items];
  [snapshot reconfigureItemsWithIdentifiers:items];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];

  // Invalidate the layout when getting to or coming from 1 item.
  if (hadOneItem != [self hasOnlyOneItem]) {
    [_collectionView.collectionViewLayout invalidateLayout];
  }

  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  } else {
    [self removeEmptyStateAnimated:YES];
  }
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  TabGroupsPanelItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  [self.mutator selectTabGroupsPanelItem:item];
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.UIDelegate tabGroupsPanelViewControllerDidScroll:self];
}

- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView*)scrollView {
  _emptyStateView.scrollViewContentInsets = scrollView.contentInset;
}

#pragma mark Private

// Returns whether the data source has only one item. It's used to configure the
// layout.
- (BOOL)hasOnlyOneItem {
  return _dataSource.snapshot.numberOfItems == 1;
}

// Returns the compositional layout for the Tab Groups panel.
- (UICollectionViewLayout*)createLayout {
  __weak __typeof(self) weakSelf = self;
  UICollectionViewLayout* layout = [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:^(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return [weakSelf makeSectionWithLayoutEnvironment:layoutEnvironment];
      }];
  return layout;
}

// Returns the layout section for the Tab Groups panel.
- (NSCollectionLayoutSection*)makeSectionWithLayoutEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  const BOOL onlyOneItem = [self hasOnlyOneItem];
  const CGFloat width = layoutEnvironment.container.effectiveContentSize.width;
  const BOOL hasLargeWidth = width > kColumnCountWidthThreshold;
  const NSInteger columnCount = hasLargeWidth && !onlyOneItem ? 2 : 1;

  NSCollectionLayoutDimension* itemWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1. / columnCount];
  NSCollectionLayoutDimension* itemHeight =
      [NSCollectionLayoutDimension estimatedDimension:kEstimatedItemHeight];
  NSCollectionLayoutSize* itemSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:itemWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutDimension* groupWidth =
      [NSCollectionLayoutDimension fractionalWidthDimension:1];
  NSCollectionLayoutSize* groupSize =
      [NSCollectionLayoutSize sizeWithWidthDimension:groupWidth
                                     heightDimension:itemHeight];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                            repeatingSubitem:item
                                                       count:columnCount];
  group.interItemSpacing =
      [NSCollectionLayoutSpacing fixedSpacing:kInterItemSpacing];

  CGFloat groupHorizontalInset = kHorizontalInset;
  const BOOL hasLargeInset =
      layoutEnvironment.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassRegular;
  if (hasLargeInset) {
    groupHorizontalInset =
        width * kHorizontalInsetPercentageWhenLargeAndTwoItems;
    if (hasLargeWidth && onlyOneItem) {
      groupHorizontalInset =
          width * kHorizontalInsetPercentageWhenLargeAndOneItem;
    }
  }
  group.contentInsets = NSDirectionalEdgeInsetsMake(0, groupHorizontalInset, 0,
                                                    groupHorizontalInset);

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = kInterGroupSpacing;
  const CGFloat sectionVerticalInset =
      hasLargeInset ? kLargeVerticalInset : kVerticalInset;
  // Use the `_contentInsets` horizontal insets. See `setContentInsets:` for
  // more details.
  section.contentInsets = NSDirectionalEdgeInsetsMake(
      sectionVerticalInset, self.contentInsets.left, sectionVerticalInset,
      self.contentInsets.right);
  return section;
}

// Whether to show the empty state view.
- (BOOL)shouldShowEmptyState {
  return _dataSource.snapshot.numberOfItems == 0;
}

// Animates the empty state into view.
- (void)animateEmptyStateIn {
  // TODO(crbug.com/40566436) : Polish the animation, and put constants where
  // they belong.
  [_emptyStateAnimator stopAnimation:YES];
  UIView* emptyStateView = _emptyStateView;
  _emptyStateAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:1.0 - _emptyStateView.alpha
          dampingRatio:1.0
            animations:^{
              emptyStateView.alpha = 1.0;
              emptyStateView.transform = CGAffineTransformIdentity;
            }];
  [_emptyStateAnimator startAnimation];
}

// Removes the empty state out of view, with animation if `animated` is YES.
- (void)removeEmptyStateAnimated:(BOOL)animated {
  // TODO(crbug.com/40566436) : Polish the animation, and put constants where
  // they belong.
  [_emptyStateAnimator stopAnimation:YES];
  UIView* emptyStateView = _emptyStateView;
  auto removeEmptyState = ^{
    emptyStateView.alpha = 0.0;
    emptyStateView.transform = CGAffineTransformScale(CGAffineTransformIdentity,
                                                      /*sx=*/0.9, /*sy=*/0.9);
  };
  if (animated) {
    _emptyStateAnimator =
        [[UIViewPropertyAnimator alloc] initWithDuration:_emptyStateView.alpha
                                            dampingRatio:1.0
                                              animations:removeEmptyState];
    [_emptyStateAnimator startAnimation];
  } else {
    removeEmptyState();
  }
}

- (void)configureCell:(TabGroupsPanelCell*)cell
             withItem:(TabGroupsPanelItem*)item {
  cell.item = item;
  TabGroupsPanelItemData* itemData =
      [_itemDataSource dataForItem:item
          withFaviconsFetchCompletion:^(NSArray<UIImage*>* favicons) {
            if ([cell.item isEqual:item]) {
              cell.faviconsGrid.favicons = favicons;
            }
          }];
  cell.titleLabel.text = itemData.title;
  cell.dot.backgroundColor = itemData.color;
  cell.subtitleLabel.text = itemData.creationText;
  cell.faviconsGrid.numberOfTabs = itemData.numberOfTabs;
}

@end

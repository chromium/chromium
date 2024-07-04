// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/saved_tab_groups/string_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_favicon_grid.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mutator.h"

namespace {

// Layout.
const CGFloat kInterItemSpacing = 24;
const CGFloat kInterGroupSpacing = 16;
const CGFloat kEstimatedItemHeight = 106;
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

// Returns a user-friendly localized string representing the duration since the
// creation date.
NSString* CreationText(base::Time creation_date) {
  return base::SysUTF16ToNSString(tab_groups::LocalizedElapsedTimeSinceCreation(
      base::Time::Now() - creation_date));
}

}  // namespace

@interface TabGroupsPanelViewController () <UICollectionViewDelegate>
@end

@implementation TabGroupsPanelViewController {
  UICollectionView* _collectionView;
  UICollectionViewDiffableDataSource<NSString*, TabGroupsPanelItem*>*
      _dataSource;
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
  _collectionView.delegate = self;
  _collectionView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:_collectionView];

  UICollectionViewCellRegistration* registration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[TabGroupsPanelCell class]
               configurationHandler:^(TabGroupsPanelCell* cell,
                                      NSIndexPath* indexPath,
                                      TabGroupsPanelItem* item) {
                 cell.titleLabel.text = item.title;
                 cell.subtitleLabel.text = CreationText(item.creationDate);
                 cell.dot.backgroundColor = item.color;
                 cell.faviconsGrid.favicons = item.favicons;
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
  [snapshot reconfigureItemsWithIdentifiers:items];  // TODO: doesn't
                                                     // reconfigure properly
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];

  // Invalidate the layout when getting to or coming from 1 item.
  if (hadOneItem != [self hasOnlyOneItem]) {
    [_collectionView.collectionViewLayout invalidateLayout];
  }

  // TODO(crbug.com/349788953): Show the empty state view if needed.
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

@end

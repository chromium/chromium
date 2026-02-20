// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view_layout.h"

#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_layout_util.h"

namespace {

/// Maximum number of items that should be fully visible on the screen.
const NSUInteger kMaximumVisibleItemsOnScreen = 4;

/// Multiplier for peeking the first off-screen element.
const CGFloat kPeekInsetMultiplerCompactWidth = 0.6;
const CGFloat kPeekInsetMultiplerRegularWidth = 0.85;

/// Peeking inset for the first off-screen element.
CGFloat PeekInsetForCollectionView(UITraitCollection* trait_collection) {
  CGFloat peek_inset_multiplier =
      trait_collection.horizontalSizeClass == UIUserInterfaceSizeClassCompact
          ? kPeekInsetMultiplerCompactWidth
          : kPeekInsetMultiplerRegularWidth;
  return kMagicStackImageContainerWidth * peek_inset_multiplier;
}

/// Creates a section in the collection view layout.
NSCollectionLayoutSection* GetSectionForMostVisitedTilesCollectionView(
    NSUInteger item_count,
    CGFloat container_width,
    UITraitCollection* trait_collection) {
  CGFloat items_per_group = MIN(item_count, kMaximumVisibleItemsOnScreen);
  NSCollectionLayoutDimension* estimated_height_dimension =
      [NSCollectionLayoutDimension estimatedDimension:kMostVisitedTileIconSize];

  NSCollectionLayoutDimension* item_width_dimension =
      [NSCollectionLayoutDimension
          fractionalWidthDimension:1 / items_per_group];
  NSCollectionLayoutItem* item = [NSCollectionLayoutItem
      itemWithLayoutSize:
          [NSCollectionLayoutSize
              sizeWithWidthDimension:item_width_dimension
                     heightDimension:estimated_height_dimension]];
  /// Group configuration.
  CGFloat group_width = container_width - kMagicStackContainerInsets.leading -
                        kMagicStackContainerInsets.trailing;
  if (item_count > kMaximumVisibleItemsOnScreen) {
    /// Allow peeking the 5th element.
    group_width -= PeekInsetForCollectionView(trait_collection);
  }
  NSCollectionLayoutDimension* group_width_dimension =
      [NSCollectionLayoutDimension absoluteDimension:group_width];
  NSCollectionLayoutGroup* group = [NSCollectionLayoutGroup
      horizontalGroupWithLayoutSize:
          [NSCollectionLayoutSize
              sizeWithWidthDimension:group_width_dimension
                     heightDimension:estimated_height_dimension]
                   repeatingSubitem:item
                              count:items_per_group];
  CGFloat spacing = ContentSuggestionsTilesHorizontalSpacing(trait_collection);
  group.interItemSpacing = [NSCollectionLayoutSpacing fixedSpacing:spacing];
  /// Section configuration.
  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.orthogonalScrollingBehavior =
      UICollectionLayoutSectionOrthogonalScrollingBehaviorContinuous;
  section.interGroupSpacing = spacing;
  section.contentInsets = kMagicStackContainerInsets;
  return section;
}

}  // namespace

@implementation MostVisitedTilesCollectionViewLayout

- (instancetype)initWithItemCount:(NSUInteger)count {
  UICollectionViewCompositionalLayoutConfiguration* config =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
  UICollectionViewCompositionalLayoutSectionProvider sectionProvider =
      ^NSCollectionLayoutSection*(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return GetSectionForMostVisitedTilesCollectionView(
            count, layoutEnvironment.container.contentSize.width,
            layoutEnvironment.traitCollection);
      };
  return [super initWithSectionProvider:sectionProvider configuration:config];
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
  return YES;
}

@end

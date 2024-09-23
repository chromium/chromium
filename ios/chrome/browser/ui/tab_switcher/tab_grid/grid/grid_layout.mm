// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {

// Items aspect ratios.
constexpr CGFloat kPortraitAspectRatio = 4. / 3.;
// Percentage of the grid width dedicated to spacing.
constexpr CGFloat kSpacingPercentage = 0.12;
// Specific spacing for iPhone Portrait.
constexpr CGFloat kIPhonePortraitSpacing = 16;
constexpr CGFloat kMinimumSpacing = kIPhonePortraitSpacing;
// Estimated size of the Inactive Tabs headers.
constexpr CGFloat kInactiveTabsHeaderEstimatedHeight = 100;
// Bottom inset of the section containing the inactive tabs button.
constexpr CGFloat kInactiveTabsSectionBottomInset = 10;
// Estimated size of the Tab Group headers.
constexpr CGFloat kTabGroupHeaderEstimatedHeight = 70;
// Estimated size of the Search headers.
constexpr CGFloat kSearchHeaderEstimatedHeight = 50;
// Estimated size of the SuggestedActions item.
constexpr CGFloat kSuggestedActionsEstimatedHeight = 100;
constexpr CGFloat kLegacySuggestedActionsEstimatedHeight = 150;
// Different width thresholds for determining the columns count.
constexpr CGFloat kSmallWidthThreshold = 500;
constexpr CGFloat kLargeWidthThreshold = 1000;
// Magic padding to fit UX mocks for items sizes. See where it is used for more
// details.
constexpr CGFloat kMagicPadding = 50;

// Short helper for a fractional width NSCollectionLayoutDimension.
NSCollectionLayoutDimension* FractionalWidth(CGFloat value) {
  return [NSCollectionLayoutDimension fractionalWidthDimension:value];
}

// Short helper for a fractional height NSCollectionLayoutDimension.
NSCollectionLayoutDimension* FractionalHeight(CGFloat value) {
  return [NSCollectionLayoutDimension fractionalHeightDimension:value];
}

// Short helper for an estimated NSCollectionLayoutDimension.
NSCollectionLayoutDimension* EstimatedDimension(CGFloat value) {
  return [NSCollectionLayoutDimension estimatedDimension:value];
}

// Short helper for an estimated NSCollectionLayoutDimension.
NSCollectionLayoutDimension* AbsoluteDimension(CGFloat value) {
  return [NSCollectionLayoutDimension absoluteDimension:value];
}

// Returns the number of columns based on the layout environment.
NSInteger ColumnsCount(id<NSCollectionLayoutEnvironment> layout_environment) {
  const CGFloat width = layout_environment.container.effectiveContentSize.width;
  const UIContentSizeCategory content_size_category =
      layout_environment.traitCollection.preferredContentSizeCategory;

  NSInteger count;
  if (width < kSmallWidthThreshold) {
    count = 2;
  } else if (width < kLargeWidthThreshold) {
    count = 3;
  } else {
    count = 4;
  }

  // If Dynamic Type uses an Accessibility setting, just remove a column.
  if (UIContentSizeCategoryIsAccessibilityCategory(content_size_category)) {
    count -= 1;
  }

  return count;
}

// Returns the aspect ratio (height / width) of an item based on the layout
// environment.
CGFloat ItemAspectRatio(id<NSCollectionLayoutEnvironment> layout_environment) {
  const CGFloat width = layout_environment.container.effectiveContentSize.width;
  const CGFloat height =
      layout_environment.container.effectiveContentSize.height;

  const CGRect screen_bounds = UIScreen.mainScreen.bounds;
  const CGFloat screen_aspect_ratio =
      CGRectGetHeight(screen_bounds) / CGRectGetWidth(screen_bounds);

  // On iPad Landscape with 3/4 - 1/4 Split View, the 3/4 width is just a bit
  // smaller than the height, but design-wise, a landscape aspect ratio should
  // be preferred. Pad a bit the width with a magic constant before comparing to
  // the height.
  return width + kMagicPadding > height ? screen_aspect_ratio
                                        : kPortraitAspectRatio;
}

// Returns the spacing based on the layout environment.
CGFloat Spacing(id<NSCollectionLayoutEnvironment> layout_environment) {
  // Get the grid width.
  const CGFloat width = layout_environment.container.effectiveContentSize.width;
  // Compute the total amount of space.
  const CGFloat total_spacing = width * kSpacingPercentage;
  // Compute the number of spaces.
  const NSInteger spaces_count = ColumnsCount(layout_environment) + 1;
  // Compute the theoretical size of the spacing, rounded to the nearest pixel.
  const CGFloat spacing = AlignValueToPixel(total_spacing / spaces_count);
  // Cap to a minimum spacing.
  return MAX(spacing, kMinimumSpacing);
}

// Returns a header layout item to add to Search mode sections.
NSCollectionLayoutBoundarySupplementaryItem* SearchModeHeader(
    id<NSCollectionLayoutEnvironment> layout_environment) {
  NSCollectionLayoutSize* header_size = [NSCollectionLayoutSize
      sizeWithWidthDimension:FractionalWidth(1.)
             heightDimension:EstimatedDimension(kSearchHeaderEstimatedHeight)];
  return [NSCollectionLayoutBoundarySupplementaryItem
      boundarySupplementaryItemWithLayoutSize:header_size
                                  elementKind:
                                      UICollectionElementKindSectionHeader
                                    alignment:NSRectAlignmentTopLeading];
}

// Returns a header layout item to add to the Open Tabs section as needed.
NSCollectionLayoutBoundarySupplementaryItem* InactiveTabsHeader() {
  NSCollectionLayoutDimension* height_dimension =
      EstimatedDimension(kInactiveTabsHeaderEstimatedHeight);
  NSCollectionLayoutSize* header_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:FractionalWidth(1.)
                                     heightDimension:height_dimension];
  return [NSCollectionLayoutBoundarySupplementaryItem
      boundarySupplementaryItemWithLayoutSize:header_size
                                  elementKind:
                                      UICollectionElementKindSectionHeader
                                    alignment:NSRectAlignmentTopLeading];
}

// Returns a header layout item to add to the Open Tabs section as needed.
NSCollectionLayoutBoundarySupplementaryItem* AnimatingOutHeader() {
  NSCollectionLayoutSize* header_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:FractionalWidth(1.)
                                     heightDimension:AbsoluteDimension(0.1)];
  return [NSCollectionLayoutBoundarySupplementaryItem
      boundarySupplementaryItemWithLayoutSize:header_size
                                  elementKind:
                                      UICollectionElementKindSectionHeader
                                    alignment:NSRectAlignmentTopLeading];
}

// Returns a header layout item to add to the Open Tabs section as needed.
NSCollectionLayoutBoundarySupplementaryItem* TabGroupHeader() {
  NSCollectionLayoutDimension* height_dimension =
      EstimatedDimension(kTabGroupHeaderEstimatedHeight);
  NSCollectionLayoutSize* header_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:FractionalWidth(1.)
                                     heightDimension:height_dimension];
  return [NSCollectionLayoutBoundarySupplementaryItem
      boundarySupplementaryItemWithLayoutSize:header_size
                                  elementKind:
                                      UICollectionElementKindSectionHeader
                                    alignment:NSRectAlignmentTopLeading];
}

// Returns a compositional layout grid section for the Inactive Tab button.
NSCollectionLayoutSection* InactiveTabButtonSection(
    id<NSCollectionLayoutEnvironment> layout_environment,
    NSDirectionalEdgeInsets section_insets) {
  // Use the same estimated height for the item and the group.
  NSCollectionLayoutDimension* estimated_height_dimension =
      EstimatedDimension(kInactiveTabsHeaderEstimatedHeight);

  // Configure the layout item.
  NSCollectionLayoutSize* item_size = [NSCollectionLayoutSize
      sizeWithWidthDimension:FractionalWidth(1.)
             heightDimension:estimated_height_dimension];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:item_size];

  // Configure the layout group.
  NSCollectionLayoutSize* group_size = [NSCollectionLayoutSize
      sizeWithWidthDimension:FractionalWidth(1.)
             heightDimension:estimated_height_dimension];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:group_size
                                                    subitems:@[ item ]];

  // Configure the layout section.
  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  const CGFloat spacing = Spacing(layout_environment);
  section_insets.top += spacing;
  section_insets.leading += spacing;
  section_insets.bottom += kInactiveTabsSectionBottomInset;
  section_insets.trailing += spacing;
  section.contentInsets = section_insets;
  section.contentInsetsReference = UIContentInsetsReferenceNone;

  return section;
}

// Returns a compositional layout grid section for opened tabs.
NSCollectionLayoutSection* TabsSection(
    id<NSCollectionLayoutEnvironment> layout_environment,
    TabsSectionHeaderType tabs_section_header_type,
    NSDirectionalEdgeInsets section_insets) {
  // Determine the number of columns.
  NSInteger count = ColumnsCount(layout_environment);

  // Configure the layout item.
  NSCollectionLayoutDimension* item_width_dimension =
      FractionalWidth(1. / count);
  const CGFloat item_aspect_ratio = ItemAspectRatio(layout_environment);
  NSCollectionLayoutDimension* item_height_dimension =
      FractionalWidth(item_aspect_ratio / count);
  NSCollectionLayoutSize* item_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:item_width_dimension
                                     heightDimension:item_height_dimension];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:item_size];

  // Configure the layout group.
  const CGFloat width = layout_environment.container.effectiveContentSize.width;
  // Estimate the height of the group by ignoring spacings.
  NSCollectionLayoutDimension* group_height_dimension =
      EstimatedDimension(item_aspect_ratio * width / count);
  NSCollectionLayoutSize* group_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:FractionalWidth(1.)
                                     heightDimension:group_height_dimension];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:group_size
                                            repeatingSubitem:item
                                                       count:count];
  const CGFloat spacing = Spacing(layout_environment);
  group.interItemSpacing = [NSCollectionLayoutSpacing fixedSpacing:spacing];

  // Configure the layout section.
  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section_insets.top += spacing;
  section_insets.leading += spacing;
  section_insets.bottom += spacing;
  section_insets.trailing += spacing;
  section.contentInsets = section_insets;
  section.contentInsetsReference = UIContentInsetsReferenceNone;
  section.interGroupSpacing = spacing;

  switch (tabs_section_header_type) {
    case TabsSectionHeaderType::kNone:
      break;
    case TabsSectionHeaderType::kSearch:
      section.boundarySupplementaryItems =
          @[ SearchModeHeader(layout_environment) ];
      break;
    case TabsSectionHeaderType::kInactiveTabs:
      section.boundarySupplementaryItems = @[ InactiveTabsHeader() ];
      break;
    case TabsSectionHeaderType::kAnimatingOut:
      section.boundarySupplementaryItems = @[ AnimatingOutHeader() ];
      break;
    case TabsSectionHeaderType::kTabGroup:
      section.boundarySupplementaryItems = @[ TabGroupHeader() ];
      break;
  }

  return section;
}

// Returns a compositional layout section for Suggested Actions.
NSCollectionLayoutSection* SuggestedActionsSection(
    id<NSCollectionLayoutEnvironment> layout_environment,
    NSDirectionalEdgeInsets section_insets) {
  // Configure the layout item.
  NSCollectionLayoutSize* item_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:FractionalWidth(1.)
                                     heightDimension:FractionalHeight(1.)];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:item_size];

  // Configure the layout group.
  CGFloat estimated_height = IsTabGroupSyncEnabled()
                                 ? kSuggestedActionsEstimatedHeight
                                 : kLegacySuggestedActionsEstimatedHeight;
  NSCollectionLayoutDimension* group_height_dimension =
      EstimatedDimension(estimated_height);
  NSCollectionLayoutSize* group_size =
      [NSCollectionLayoutSize sizeWithWidthDimension:FractionalWidth(1.)
                                     heightDimension:group_height_dimension];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:group_size
                                                    subitems:@[ item ]];

  // Configure the layout section.
  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  const CGFloat spacing = Spacing(layout_environment);
  section_insets.top += spacing;
  section_insets.leading += spacing;
  section_insets.bottom += spacing;
  section_insets.trailing += spacing;
  section.contentInsets = section_insets;
  section.contentInsetsReference = UIContentInsetsReferenceNone;
  section.boundarySupplementaryItems =
      @[ SearchModeHeader(layout_environment) ];

  return section;
}

}  // namespace

@implementation GridLayout {
  NSArray<NSIndexPath*>* _indexPathsOfDeletingItems;
  NSArray<NSIndexPath*>* _indexPathsOfInsertingItems;
}

- (instancetype)init {
  // Use a `futureSelf` variable as the super init requires a closure, and as
  // `self` is not instantiated yet, it can't be used.
  __block __typeof(self) futureSelf;
  self = [super initWithSectionProvider:^(
                    NSInteger sectionIndex,
                    id<NSCollectionLayoutEnvironment> layoutEnvironment) {
    return [futureSelf sectionAtIndex:sectionIndex
                    layoutEnvironment:layoutEnvironment];
  }];
  futureSelf = self;
  if (self) {
    _animatesItemUpdates = YES;
  }
  return self;
}

#pragma mark - UICollectionViewLayout

- (void)prepareForCollectionViewUpdates:
    (NSArray<UICollectionViewUpdateItem*>*)updateItems {
  [super prepareForCollectionViewUpdates:updateItems];
  // Track which items in this update are explicitly being deleted or inserted.
  NSMutableArray<NSIndexPath*>* deletingItems =
      [NSMutableArray arrayWithCapacity:updateItems.count];
  NSMutableArray<NSIndexPath*>* insertingItems =
      [NSMutableArray arrayWithCapacity:updateItems.count];
  for (UICollectionViewUpdateItem* item in updateItems) {
    switch (item.updateAction) {
      case UICollectionUpdateActionDelete:
        [deletingItems addObject:item.indexPathBeforeUpdate];
        break;
      case UICollectionUpdateActionInsert:
        [insertingItems addObject:item.indexPathAfterUpdate];
        break;
      default:
        break;
    }
  }
  _indexPathsOfDeletingItems = [deletingItems copy];
  _indexPathsOfInsertingItems = [insertingItems copy];
}

- (UICollectionViewLayoutAttributes*)
    finalLayoutAttributesForDisappearingItemAtIndexPath:
        (NSIndexPath*)itemIndexPath {
  // Return initial layout if animations are disabled.
  if (!_animatesItemUpdates) {
    return [self layoutAttributesForItemAtIndexPath:itemIndexPath];
  }
  // Note that this method is called for any item whose index path changing from
  // `itemIndexPath`, which includes any items that were in the layout and whose
  // index path is changing. For an item whose index path is changing, this
  // method is called before
  // -initialLayoutAttributesForAppearingItemAtIndexPath:
  UICollectionViewLayoutAttributes* attributes = [[super
      finalLayoutAttributesForDisappearingItemAtIndexPath:itemIndexPath] copy];
  // Disappearing items that aren't being deleted just use the default
  // attributes.
  if (![_indexPathsOfDeletingItems containsObject:itemIndexPath]) {
    return attributes;
  }
  // Cells being deleted scale to 0, and are z-positioned behind all others.
  // (Note that setting the zIndex here actually has no effect, despite what is
  // implied in the UIKit documentation).
  attributes.zIndex = -10;
  // Scaled down to 0% (or near enough).
  CGAffineTransform transform =
      CGAffineTransformScale(attributes.transform, /*sx=*/0.01, /*sy=*/0.01);
  attributes.transform = transform;
  // Fade out.
  attributes.alpha = 0.0;
  return attributes;
}

- (UICollectionViewLayoutAttributes*)
    initialLayoutAttributesForAppearingItemAtIndexPath:
        (NSIndexPath*)itemIndexPath {
  // Return final layout if animations are disabled.
  if (!_animatesItemUpdates) {
    return [self layoutAttributesForItemAtIndexPath:itemIndexPath];
  }
  // Note that this method is called for any item whose index path is becoming
  // `itemIndexPath`, which includes any items that were in the layout but whose
  // index path is changing. For an item whose index path is changing, this
  // method is called after
  // -finalLayoutAttributesForDisappearingItemAtIndexPath:
  UICollectionViewLayoutAttributes* attributes = [[super
      initialLayoutAttributesForAppearingItemAtIndexPath:itemIndexPath] copy];
  // Appearing items that aren't being inserted just use the default
  // attributes.
  if (![_indexPathsOfInsertingItems containsObject:itemIndexPath]) {
    return attributes;
  }
  // TODO(crbug.com/40566436) : Polish the animation, and put constants where
  // they belong. Cells being inserted start faded out, scaled down, and drop
  // downwards slightly.
  attributes.alpha = 0.0;
  CGAffineTransform transform =
      CGAffineTransformScale(attributes.transform, /*sx=*/0.9, /*sy=*/0.9);
  transform = CGAffineTransformTranslate(transform, /*tx=*/0,
                                         /*ty=*/attributes.size.height * 0.1);
  attributes.transform = transform;
  return attributes;
}

- (void)finalizeCollectionViewUpdates {
  _indexPathsOfDeletingItems = nil;
  _indexPathsOfInsertingItems = nil;
  [super finalizeCollectionViewUpdates];
}

- (BOOL)flipsHorizontallyInOppositeLayoutDirection {
  return UseRTLLayout() ? YES : NO;
}

#pragma mark - Private

// Returns a compositional layout grid section.
- (NSCollectionLayoutSection*)sectionAtIndex:(NSInteger)sectionIndex
                           layoutEnvironment:(id<NSCollectionLayoutEnvironment>)
                                                 layoutEnvironment {
  NSString* sectionIdentifier =
      [self.diffableDataSource sectionIdentifierForIndex:sectionIndex];
  if ([sectionIdentifier isEqualToString:kInactiveTabButtonSectionIdentifier]) {
    return InactiveTabButtonSection(layoutEnvironment, self.sectionInsets);
  } else if ([sectionIdentifier
                 isEqualToString:kGridOpenTabsSectionIdentifier]) {
    return TabsSection(layoutEnvironment, self.tabsSectionHeaderType,
                       self.sectionInsets);
  } else if ([sectionIdentifier
                 isEqualToString:kSuggestedActionsSectionIdentifier]) {
    return SuggestedActionsSection(layoutEnvironment, self.sectionInsets);
  }

  NOTREACHED();
}

@end

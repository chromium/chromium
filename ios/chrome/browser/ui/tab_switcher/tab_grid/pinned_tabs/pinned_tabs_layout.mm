// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_layout.h"

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"

@implementation PinnedTabsLayout {
  NSArray<NSIndexPath*>* _indexPathsOfDeletingItems;
  NSArray<NSIndexPath*>* _indexPathsOfInsertingItems;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.scrollDirection = UICollectionViewScrollDirectionHorizontal;
    self.minimumInteritemSpacing = kPinnedCellInteritemSpacing;
  }
  return self;
}

#pragma mark - UICollectionViewLayout

- (void)prepareLayout {
  [super prepareLayout];

  NSInteger itemCount = [self.collectionView numberOfItemsInSection:0];
  // No need to update the layout if the collectionView is empty.
  if (itemCount == 0) {
    return;
  }

  CGFloat collectionViewWidth = CGRectGetWidth(self.collectionView.bounds);
  CGFloat itemSpacingSum = self.minimumInteritemSpacing * (itemCount - 1);
  CGFloat horizontalPaddingSum = 2 * kPinnedCellHorizontalLayoutInsets;

  CGFloat itemWidth =
      (collectionViewWidth - itemSpacingSum - horizontalPaddingSum) / itemCount;
  itemWidth = MAX(itemWidth, kPinnedCellMinWidth);
  itemWidth = MIN(itemWidth, kPinnedCellMaxWidth);
  self.itemSize = CGSize{itemWidth, kPinnedCellHeight};

  // Center items if there is enough available space.
  CGFloat availableHorizontalLayoutInsets =
      (collectionViewWidth - (itemWidth * itemCount) - itemSpacingSum) / 2;
  availableHorizontalLayoutInsets =
      MAX(availableHorizontalLayoutInsets, kPinnedCellHorizontalLayoutInsets);

  self.sectionInset = UIEdgeInsets{
      kPinnedCellVerticalLayoutInsets, availableHorizontalLayoutInsets,
      kPinnedCellVerticalLayoutInsets, availableHorizontalLayoutInsets};
}

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

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
  return YES;
}

@end

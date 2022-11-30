// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/horizontal_layout.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Initial scale for items being inserted in the collection view.
CGFloat InsertedItemInitialScale = 0.01;

}  // namespace

@implementation HorizontalLayout

- (instancetype)init {
  if (self = [super init]) {
    self.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  }
  return self;
}

#pragma mark - UICollectionViewLayout

// This is called whenever the layout is invalidated, including during rotation.
// Resizes item, margins, and spacing to fit new size classes and width.
- (void)prepareLayout {
  [super prepareLayout];

  self.itemSize = kGridCellSizeSmall;
  CGFloat spacing = kGridLayoutLineSpacingCompactCompactLimitedWidth;
  // Use the height of the available content area.
  CGFloat height = CGRectGetHeight(UIEdgeInsetsInsetRect(
      self.collectionView.bounds, self.collectionView.contentInset));
  CGFloat topInset =
      kGridCellSelectionRingGapWidth + kGridCellSelectionRingTintWidth;
  self.sectionInset = UIEdgeInsets{
      topInset, spacing, height - self.itemSize.height - topInset, spacing};
  self.minimumLineSpacing = kGridLayoutLineSpacingRegularRegular;
}

- (UICollectionViewLayoutAttributes*)
    initialLayoutAttributesForAppearingItemAtIndexPath:
        (NSIndexPath*)itemIndexPath {
  if (!self.animatesItemUpdates) {
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
  if (![self.indexPathsOfInsertingItems containsObject:itemIndexPath]) {
    return attributes;
  }

  attributes.alpha = 0.0;
  attributes.transform = CGAffineTransformScale(
      attributes.transform, InsertedItemInitialScale, InsertedItemInitialScale);
  return attributes;
}

@end

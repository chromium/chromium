// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/horizontal_layout.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/reordering_layout_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  CGFloat height = CGRectGetHeight(self.collectionView.bounds);
  CGFloat spacing = kGridLayoutLineSpacingCompactCompactLimitedWidth;
  CGFloat topInset = spacing + kGridCellSelectionRingGapWidth +
                     kGridCellSelectionRingTintWidth;
  self.sectionInset = UIEdgeInsets{
      topInset, spacing, height - self.itemSize.height - 2 * topInset, spacing};
  self.minimumLineSpacing = kGridLayoutLineSpacingRegularRegular;
}

@end

@implementation HorizontalReorderingLayout

#pragma mark - UICollectionViewLayout

// Both -layoutAttributesForElementsInRect: and
// -layoutAttributesForItemAtIndexPath: need to be overridden to change the
// default layout attributes.
- (NSArray<__kindof UICollectionViewLayoutAttributes*>*)
    layoutAttributesForElementsInRect:(CGRect)rect {
  return CopyAttributesArrayAndSetInactiveOpacity(
      [super layoutAttributesForElementsInRect:rect]);
}

- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  return CopyAttributesAndSetInactiveOpacity(
      [super layoutAttributesForItemAtIndexPath:indexPath]);
}

- (UICollectionViewLayoutAttributes*)
    layoutAttributesForInteractivelyMovingItemAtIndexPath:
        (NSIndexPath*)indexPath
                                       withTargetPosition:(CGPoint)position {
  return CopyAttributesAndSetActiveProperties([super
      layoutAttributesForInteractivelyMovingItemAtIndexPath:indexPath
                                         withTargetPosition:position]);
}

@end

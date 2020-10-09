// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid/grid_layout.h"

#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GridLayout

#pragma mark - UICollectionViewLayout

// This is called whenever the layout is invalidated, including during rotation.
// Resizes item, margins, and spacing to fit new size classes and width.
- (void)prepareLayout {
  [super prepareLayout];

  UIUserInterfaceSizeClass horizontalSizeClass =
      self.collectionView.traitCollection.horizontalSizeClass;
  UIUserInterfaceSizeClass verticalSizeClass =
      self.collectionView.traitCollection.verticalSizeClass;
  CGFloat width = CGRectGetWidth(self.collectionView.bounds);
  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory)) {
    self.itemSize = kGridCellSizeAccessibility;
    self.sectionInset = kGridLayoutInsetsRegularCompact;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularCompact;
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
             verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    self.itemSize = kGridCellSizeSmall;
    if (width < kGridLayoutCompactCompactLimitedWidth) {
      self.sectionInset = kGridLayoutInsetsCompactCompactLimitedWidth;
      self.minimumLineSpacing =
          kGridLayoutLineSpacingCompactCompactLimitedWidth;
    } else {
      self.sectionInset = kGridLayoutInsetsCompactCompact;
      self.minimumLineSpacing = kGridLayoutLineSpacingCompactCompact;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
             verticalSizeClass == UIUserInterfaceSizeClassRegular) {
    if (width < kGridLayoutCompactRegularLimitedWidth) {
      self.itemSize = kGridCellSizeSmall;
      self.sectionInset = kGridLayoutInsetsCompactRegularLimitedWidth;
      self.minimumLineSpacing =
          kGridLayoutLineSpacingCompactRegularLimitedWidth;
    } else {
      self.itemSize = kGridCellSizeMedium;
      self.sectionInset = kGridLayoutInsetsCompactRegular;
      self.minimumLineSpacing = kGridLayoutLineSpacingCompactRegular;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassRegular &&
             verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    self.itemSize = kGridCellSizeSmall;
    self.sectionInset = kGridLayoutInsetsRegularCompact;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularCompact;
  } else {
    self.itemSize = kGridCellSizeLarge;
    self.sectionInset = kGridLayoutInsetsRegularRegular;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularRegular;
  }
}

@end

@implementation GridReorderingLayout

#pragma mark - UICollectionViewLayout

// Both -layoutAttributesForElementsInRect: and
// -layoutAttributesForItemAtIndexPath: need to be overridden to change the
// default layout attributes.
- (NSArray<__kindof UICollectionViewLayoutAttributes*>*)
    layoutAttributesForElementsInRect:(CGRect)rect {
  NSArray* baseAttributes = [super layoutAttributesForElementsInRect:rect];
  NSMutableArray<__kindof UICollectionViewLayoutAttributes*>* attributes =
      [NSMutableArray array];
  for (UICollectionViewLayoutAttributes* attribute in baseAttributes) {
    UICollectionViewLayoutAttributes* newAttribute = [attribute copy];
    newAttribute.alpha = kReorderingInactiveCellOpacity;
    [attributes addObject:newAttribute];
  }
  return [attributes copy];
}

- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  UICollectionViewLayoutAttributes* attributes =
      [[super layoutAttributesForItemAtIndexPath:indexPath] copy];
  attributes.alpha = kReorderingInactiveCellOpacity;
  return attributes;
}

- (UICollectionViewLayoutAttributes*)
    layoutAttributesForInteractivelyMovingItemAtIndexPath:
        (NSIndexPath*)indexPath
                                       withTargetPosition:(CGPoint)position {
  UICollectionViewLayoutAttributes* attributes = [[super
      layoutAttributesForInteractivelyMovingItemAtIndexPath:indexPath
                                         withTargetPosition:position] copy];
  // The moving item has regular opacity, but is scaled.
  attributes.alpha = 1.0;
  attributes.transform =
      CGAffineTransformScale(attributes.transform, kReorderingActiveCellScale,
                             kReorderingActiveCellScale);
  return attributes;
}

@end

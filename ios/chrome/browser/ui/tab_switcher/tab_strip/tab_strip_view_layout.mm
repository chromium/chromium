// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tab dimensions.
const CGFloat kTabOverlapStacked = 32.0;
const CGFloat kMinTabWidthStacked = 200.0;

}  // namespace

@implementation TabStripViewLayout

- (CGSize)collectionViewContentSize {
  UICollectionView* collection = self.collectionView;
  NSInteger num = [collection numberOfItemsInSection:0];

  CGFloat width = kMinTabWidthStacked * num;
  width -= (num - 1) * kTabOverlapStacked;
  width = MAX(width, collection.bounds.size.width);
  return CGSizeMake(width, collection.bounds.size.height);
}

// Retrieves layout information for an item at the specified index path
// with a corresponding cell.
- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  UICollectionView* collection = self.collectionView;
  CGRect bounds = collection.bounds;

  // Calculating tab's length size depending on the number of tabs.
  CGFloat x = indexPath.row * kMinTabWidthStacked;
  if (indexPath.row > 0) {
    x -= (kTabOverlapStacked * indexPath.row);
  }

  x = MAX(x, bounds.origin.x);

  UICollectionViewLayoutAttributes* attr = [UICollectionViewLayoutAttributes
      layoutAttributesForCellWithIndexPath:indexPath];
  attr.frame =
      CGRectMake(x, bounds.origin.y, kMinTabWidthStacked, bounds.size.height);
  return attr;
}

- (NSArray<UICollectionViewLayoutAttributes*>*)
    layoutAttributesForElementsInRect:(CGRect)rect {
  NSInteger count = [self.collectionView numberOfItemsInSection:0];

  NSMutableArray* result =
      [NSMutableArray arrayWithCapacity:CGRectGetWidth(rect) / 190];
  CGFloat x = CGRectGetMinX(rect);
  NSInteger i = 0;
  while (x < CGRectGetMaxX(rect) && i < count) {
    // Modifies Layout attributes.
    UICollectionViewLayoutAttributes* attr = [self
        layoutAttributesForItemAtIndexPath:[NSIndexPath indexPathForRow:i
                                                              inSection:0]];
    [result addObject:attr];
    x = CGRectGetMaxX(attr.frame);
    i++;
  }
  return result;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
  return YES;
}

- (UICollectionViewLayoutAttributes*)
    initialLayoutAttributesForAppearingItemAtIndexPath:
        (NSIndexPath*)itemIndexPath {
  UICollectionViewLayoutAttributes* attr =
      [self layoutAttributesForItemAtIndexPath:itemIndexPath];
  CGRect frame = attr.frame;
  frame.origin.y = CGRectGetMaxY(frame);
  attr.frame = frame;
  return attr;
}

@end

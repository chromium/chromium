// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_layout.h"

#import <algorithm>

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tab dimensions.
const CGFloat kTabOverlap = 32.0;
const CGFloat kNewTabOverlap = 13.0;
const CGFloat kMaxTabWidth = 265.0;
const CGFloat kMinTabWidth = 200.0;

// The size of the new tab button.
const CGFloat kNewTabButtonWidth = 44;

}  // namespace

@interface TabStripViewLayout ()

// The current tab width.  Recomputed whenever a tab is added or removed.
@property(nonatomic) CGFloat currentTabWidth;

@end

@implementation TabStripViewLayout

- (CGSize)collectionViewContentSize {
  UICollectionView* collection = self.collectionView;
  NSInteger num = [collection numberOfItemsInSection:0];

  CGFloat visibleSpace = [self tabStripVisibleSpace];
  _currentTabWidth = (visibleSpace + (kTabOverlap * (num - 1))) / num;
  _currentTabWidth = std::clamp(_currentTabWidth, kMinTabWidth, kMaxTabWidth);

  CGFloat width = _currentTabWidth * num - (num - 1) * kTabOverlap;
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
  CGFloat x = indexPath.row * _currentTabWidth;
  if (indexPath.row > 0) {
    x -= (kTabOverlap * indexPath.row);
  }

  x = MAX(x, bounds.origin.x);

  UICollectionViewLayoutAttributes* attr = [UICollectionViewLayoutAttributes
      layoutAttributesForCellWithIndexPath:indexPath];
  attr.frame =
      CGRectMake(x, bounds.origin.y, _currentTabWidth, bounds.size.height);
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

#pragma mark - Private

// The available space for the tabstrip which is the view width without newtab-
// button width.
- (CGFloat)tabStripVisibleSpace {
  CGFloat availableSpace = CGRectGetWidth([self.collectionView bounds]) -
                           kNewTabButtonWidth + kNewTabOverlap;
  return availableSpace;
}

@end

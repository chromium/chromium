// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_layout.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PinnedTabsLayout

- (instancetype)init {
  if (self = [super init]) {
    self.scrollDirection = UICollectionViewScrollDirectionHorizontal;
    self.minimumInteritemSpacing = kPinnedCellInteritemSpacing;
  }
  return self;
}

#pragma mark - UICollectionViewLayout

- (void)prepareLayout {
  [super prepareLayout];

  NSInteger itemCount = [self.collectionView numberOfItemsInSection:0];
  // No neeed to update the layout if the collectionView is empty.
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

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
  return YES;
}

@end

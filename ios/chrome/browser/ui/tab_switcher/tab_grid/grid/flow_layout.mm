// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/flow_layout.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FlowLayout ()
@property(nonatomic, strong) NSArray<NSIndexPath*>* indexPathsOfDeletingItems;
@property(nonatomic, strong) NSArray<NSIndexPath*>* indexPathsOfInsertingItems;
@end

@implementation FlowLayout

- (instancetype)init {
  if (self = [super init]) {
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
  self.indexPathsOfDeletingItems = [deletingItems copy];
  self.indexPathsOfInsertingItems = [insertingItems copy];
}

- (UICollectionViewLayoutAttributes*)
    finalLayoutAttributesForDisappearingItemAtIndexPath:
        (NSIndexPath*)itemIndexPath {
  // Return initial layout if animations are disabled.
  if (!self.animatesItemUpdates) {
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
  if (![self.indexPathsOfDeletingItems containsObject:itemIndexPath]) {
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
  // TODO(crbug.com/820410) : Polish the animation, and put constants where they
  // belong.
  // Cells being inserted start faded out, scaled down, and drop downwards
  // slightly.
  attributes.alpha = 0.0;
  CGAffineTransform transform =
      CGAffineTransformScale(attributes.transform, /*sx=*/0.9, /*sy=*/0.9);
  transform = CGAffineTransformTranslate(transform, /*tx=*/0,
                                         /*ty=*/attributes.size.height * 0.1);
  attributes.transform = transform;
  return attributes;
}

- (void)finalizeCollectionViewUpdates {
  self.indexPathsOfDeletingItems = @[];
  self.indexPathsOfInsertingItems = @[];
  [super finalizeCollectionViewUpdates];
}

- (BOOL)flipsHorizontallyInOppositeLayoutDirection {
  return UseRTLLayout() ? YES : NO;
}

@end

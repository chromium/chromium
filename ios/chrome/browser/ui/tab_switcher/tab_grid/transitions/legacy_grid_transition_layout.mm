// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_to_tab_transition_view.h"

@interface LegacyGridTransitionLayout ()
@property(nonatomic, readwrite)
    NSArray<LegacyGridTransitionItem*>* inactiveItems;
@property(nonatomic, readwrite) LegacyGridTransitionActiveItem* activeItem;
@property(nonatomic, readwrite) LegacyGridTransitionItem* selectionItem;
@end

@implementation LegacyGridTransitionLayout
@synthesize activeItem = _activeItem;
@synthesize selectionItem = _selectionItem;
@synthesize inactiveItems = _inactiveItems;
@synthesize expandedRect = _expandedRect;
@synthesize frameChanged = _frameChanged;

+ (instancetype)
    layoutWithInactiveItems:(NSArray<LegacyGridTransitionItem*>*)items
                 activeItem:(LegacyGridTransitionActiveItem*)activeItem
              selectionItem:(LegacyGridTransitionItem*)selectionItem {
  DCHECK(items);
  LegacyGridTransitionLayout* layout =
      [[LegacyGridTransitionLayout alloc] init];
  layout.inactiveItems = items;
  layout.activeItem = activeItem;
  layout.selectionItem = selectionItem;
  return layout;
}

@end

@interface LegacyGridTransitionItem ()
@property(nonatomic, readwrite) UIView* cell;
@property(nonatomic, readwrite) CGPoint center;
@end

@implementation LegacyGridTransitionItem
@synthesize cell = _cell;
@synthesize center = _center;

+ (instancetype)itemWithCell:(UIView*)cell center:(CGPoint)center {
  DCHECK(cell);
  DCHECK(!cell.superview);
  LegacyGridTransitionItem* item = [[self alloc] init];
  item.cell = cell;
  item.center = center;
  return item;
}
@end

@interface LegacyGridTransitionActiveItem ()
@property(nonatomic, readwrite) UIView<LegacyGridToTabTransitionView>* cell;
@property(nonatomic, readwrite) CGSize size;
@end

@implementation LegacyGridTransitionActiveItem
@dynamic cell;
@synthesize size = _size;
@synthesize isAppearing = _isAppearing;

+ (instancetype)itemWithCell:(UIView<LegacyGridToTabTransitionView>*)cell
                      center:(CGPoint)center
                        size:(CGSize)size {
  LegacyGridTransitionActiveItem* item = [self itemWithCell:cell center:center];
  item.size = size;
  return item;
}

- (void)populateWithSnapshotsFromView:(UIView*)view middleRect:(CGRect)rect {
  self.cell.mainTabView = [view resizableSnapshotViewFromRect:rect
                                           afterScreenUpdates:YES
                                                withCapInsets:UIEdgeInsetsZero];
  CGSize viewSize = view.bounds.size;
  if (rect.origin.y > 0) {
    // `rect` starts below the top of `view`, so section off the top part of
    // `view`.
    CGRect topRect = CGRectMake(0, 0, viewSize.width, rect.origin.y);
    self.cell.topTabView =
        [view resizableSnapshotViewFromRect:topRect
                         afterScreenUpdates:YES
                              withCapInsets:UIEdgeInsetsZero];
  }
  CGFloat middleRectBottom = CGRectGetMaxY(rect);
  if (middleRectBottom < viewSize.height) {
    // `rect` ends above the bottom of `view`, so section off the bottom part of
    // `view`.
    CGFloat bottomHeight = viewSize.height - middleRectBottom;
    CGRect bottomRect =
        CGRectMake(0, middleRectBottom, viewSize.width, bottomHeight);
    self.cell.bottomTabView =
        [view resizableSnapshotViewFromRect:bottomRect
                         afterScreenUpdates:YES
                              withCapInsets:UIEdgeInsetsZero];
  }
}

@end

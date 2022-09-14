// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_to_tab_transition_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/check.h"

@interface GridTransitionLayout ()
@property(nonatomic, readwrite) NSArray<GridTransitionItem*>* inactiveItems;
@property(nonatomic, readwrite) GridTransitionActiveItem* activeItem;
@property(nonatomic, readwrite) GridTransitionItem* selectionItem;
@end

@implementation GridTransitionLayout
@synthesize activeItem = _activeItem;
@synthesize selectionItem = _selectionItem;
@synthesize inactiveItems = _inactiveItems;
@synthesize expandedRect = _expandedRect;
@synthesize frameChanged = _frameChanged;

+ (instancetype)layoutWithInactiveItems:(NSArray<GridTransitionItem*>*)items
                             activeItem:(GridTransitionActiveItem*)activeItem
                          selectionItem:(GridTransitionItem*)selectionItem {
  DCHECK(items);
  GridTransitionLayout* layout = [[GridTransitionLayout alloc] init];
  layout.inactiveItems = items;
  layout.activeItem = activeItem;
  layout.selectionItem = selectionItem;
  return layout;
}

@end

@interface GridTransitionItem ()
@property(nonatomic, readwrite) UIView* cell;
@property(nonatomic, readwrite) CGPoint center;
@end

@implementation GridTransitionItem
@synthesize cell = _cell;
@synthesize center = _center;

+ (instancetype)itemWithCell:(UIView*)cell center:(CGPoint)center {
  DCHECK(cell);
  DCHECK(!cell.superview);
  GridTransitionItem* item = [[self alloc] init];
  item.cell = cell;
  item.center = center;
  return item;
}
@end

@interface GridTransitionActiveItem ()
@property(nonatomic, readwrite) UIView<GridToTabTransitionView>* cell;
@property(nonatomic, readwrite) CGSize size;
@end

@implementation GridTransitionActiveItem
@dynamic cell;
@synthesize size = _size;
@synthesize isAppearing = _isAppearing;

+ (instancetype)itemWithCell:(UIView<GridToTabTransitionView>*)cell
                      center:(CGPoint)center
                        size:(CGSize)size {
  GridTransitionActiveItem* item = [self itemWithCell:cell center:center];
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

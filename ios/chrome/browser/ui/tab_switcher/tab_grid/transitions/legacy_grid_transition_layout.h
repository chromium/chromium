// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_LAYOUT_H_

#import <UIKit/UIKit.h>

@protocol LegacyGridToTabTransitionView;
@class LegacyGridTransitionActiveItem;
@class LegacyGridTransitionItem;

// An encapsulation of information for the layout of a grid of cells that will
// be used in an animated transition. The layout object is composed of layout
// items (see below).
@interface LegacyGridTransitionLayout : NSObject

// The inactive items in the layout. `activeItem` and `selectionItem` are not
// in this array.
@property(nonatomic, copy, readonly)
    NSArray<LegacyGridTransitionItem*>* inactiveItems;

// The item in the layout (if any) that's the 'active' item (the one that will
// expand and contract).
@property(nonatomic, strong, readonly)
    LegacyGridTransitionActiveItem* activeItem;

// An item that shows the selections state (and nothing else) of the active
// item.
@property(nonatomic, strong, readonly) LegacyGridTransitionItem* selectionItem;

// The rect, in UIWindow coordinates, that an "expanded" item should occupy.
@property(nonatomic, assign) CGRect expandedRect;

// YES if the initial frame of the grid is different from the frame used for
// the animation.
@property(nonatomic, assign) BOOL frameChanged;

// Creates a new layout object.
// `inactiveItems` should be non-nil, but it may be empty.
// `activeItem` and `selectionItem` may be nil.
+ (instancetype)
    layoutWithInactiveItems:(NSArray<LegacyGridTransitionItem*>*)items
                 activeItem:(LegacyGridTransitionActiveItem*)activeItem
              selectionItem:(LegacyGridTransitionItem*)selectionItem;

@end

// An encapsulation of information for the layout of a single grid cell.
@interface LegacyGridTransitionItem : NSObject

// A view with the desired appearance of the cell for animation. This should
// not be in any view hierarchy when the layout item is created. It should
// otherwise be sized correctly and have the correct appearance.
@property(nonatomic, strong, readonly) UIView* cell;

// The position of `cell` in the grid view, normalized to UIWindow coordinates.
@property(nonatomic, readonly) CGPoint center;

// Creates a new layout item instance with `cell` and `center`. It's the
// responsibility of the caller to normalize `center` to UIWindow coordinates.
// It's an error if `cell` has a superview or is nil.
+ (instancetype)itemWithCell:(UIView*)cell center:(CGPoint)center;

@end

// An extension of LegacyGridTransitionItem for an item that will transition
// between 'cell' and 'tab' appearance during the animation.
@interface LegacyGridTransitionActiveItem : LegacyGridTransitionItem

// A view with the desired appearance of the cell for animation. This should
// not be in any view hierarchy when the layout item is created. It should
// otherwise be sized correctly and have the correct appearance.
@property(nonatomic, strong, readonly)
    UIView<LegacyGridToTabTransitionView>* cell;

// The size of `cell` in the grid.
@property(nonatomic, readonly) CGSize size;

// YES if the item is "appearing" in the grid as part of this animation.
@property(nonatomic, assign) BOOL isAppearing;

// Creates a new active item instance with `cell`, `center` and `size`.
+ (instancetype)itemWithCell:(UIView<LegacyGridToTabTransitionView>*)cell
                      center:(CGPoint)center
                        size:(CGSize)size;

// Populate the `cell` view of the receiver by extracting snapshots from `view`,
// using `rect` to define (in `view`'s coordinates) the main tab view, with any
// space above and below `rect` being the top and bottom tab views.
- (void)populateWithSnapshotsFromView:(UIView*)view middleRect:(CGRect)rect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_LAYOUT_H_

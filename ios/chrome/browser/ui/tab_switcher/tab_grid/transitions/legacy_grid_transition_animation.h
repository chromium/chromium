// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_ANIMATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_ANIMATION_H_

#import <UIKit/UIKit.h>

@class LegacyGridTransitionLayout;

// The directions the animation can take.
typedef NS_ENUM(NSUInteger, GridAnimationDirection) {
  // Moving from an expanded single tab down into the grid.
  GridAnimationDirectionContracting = 0,
  // Moving from the grid out to an expanded single tab.
  GridAnimationDirectionExpanding = 1,
};

// A view that encapsulates an animation used to transition into a grid.
// A transition animator should place this view at the appropriate place in the
// view hierarchy and then call `-beginAnimations` on its `animator` property.
@interface LegacyGridTransitionAnimation : UIView

// The animator object this animation uses; it will have the same duration
// that this object is initialized with.
// This property is `nil` until this object is added to a view hierarchy. Any
// animations or callbacks added to `animator` must be added *after* this object
// is added as a subview of another view.
@property(nonatomic, readonly) id<UIViewImplicitlyAnimating> animator;

// The active cell view; this will be animated to or from the `expandedRect`
// specified by the GridTransitionLayout this object is initialized with, so
// it may be necessary to reparent `activeItem` to another view so the
// animation can be properly layered.
@property(nonatomic, readonly) UIView* activeItem;

// The selected cell view; this will be animated to or from the `expandedRect`
// specified by the GridTransitionLayout this object is initialized with, so
// it may be necessary to reparent `selectionItem` to another view so the
// animation can be properly layered.
@property(nonatomic, readonly) UIView* selectionItem;

// Designated initializer. `layout` is a GridTransitionLayout object defining
// the layout the animation should animate to. `gridContainerFrame` is the
// frame, in the future superview coordinates, of the grid, used to avoid
// displaying tabs outside of the grid. `delegate` is an object that will be
// informed about events in this object's animation. `direction` is the
// direction that the transition will animate.
- (instancetype)initWithLayout:(LegacyGridTransitionLayout*)layout
            gridContainerFrame:(CGRect)gridContainerFrame
                      duration:(NSTimeInterval)duration
                     direction:(GridAnimationDirection)direction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_ANIMATION_H_

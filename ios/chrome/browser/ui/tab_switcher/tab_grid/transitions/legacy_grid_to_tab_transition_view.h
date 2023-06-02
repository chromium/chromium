// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TO_TAB_TRANSITION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TO_TAB_TRANSITION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_animation.h"

// An collection of properties and methods a view must support in order to be
// used to animate the transition between a grid cell and a browser tab.
@protocol LegacyGridToTabTransitionView

// The subview at the top of the view in 'cell' state.
@property(nonatomic, strong) UIView* topCellView;

// The subview at the top of the view in 'tab' state.
@property(nonatomic, strong) UIView* topTabView;

// The subview containing the main content in 'cell' state.
@property(nonatomic, strong) UIView* mainCellView;

// The subview containing the main content in 'tab' state.
@property(nonatomic, strong) UIView* mainTabView;

// The subview at the bottom of the view in 'tab' state.
@property(nonatomic, strong) UIView* bottomTabView;

// The corner radius of the view.
@property(nonatomic) CGFloat cornerRadius;

// Tells the view to set up itself for a transition with a specified
// `animationDirection`.
- (void)prepareForTransitionWithAnimationDirection:
    (GridAnimationDirection)animationDirection;

// Tells the view to scale and position its subviews for the "tab" layout. This
// must be able to be called inside an animation block.
- (void)positionTabViews;

// Tells the view to scale and position its subviews for the "cell" layout. This
// must be able to be called inside an animation block.
- (void)positionCellViews;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TO_TAB_TRANSITION_VIEW_H_

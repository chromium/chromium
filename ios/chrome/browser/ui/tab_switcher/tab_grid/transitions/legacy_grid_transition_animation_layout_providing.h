// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_ANIMATION_LAYOUT_PROVIDING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_ANIMATION_LAYOUT_PROVIDING_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_animation.h"

@class LegacyGridTransitionLayout;

// Objects conforming to this protocol can provide information for the
// animation of the transitions from and to a grid.
@protocol LegacyGridTransitionAnimationLayoutProviding

// YES if the currently selected cell is visible in the grid.
@property(nonatomic, readonly, getter=isSelectedCellVisible)
    BOOL selectedCellVisible;

// Asks the provider if the currently selected cell should be reparented to the
// topmost view. Proper view parenting/layouting is needed in order to provide
// a smooth animation from the Tab View to the Tab Grid (and vice versa).
- (BOOL)shouldReparentSelectedCell:(GridAnimationDirection)animationDirection;

// Asks the provider for the layout of the grid for the `activePage`, used in
// transition animations.
- (LegacyGridTransitionLayout*)transitionLayout:(TabGridPage)activePage;

// Asks the provider for the view to which the animation views should be added.
- (UIView*)animationViewsContainer;

// Asks the provider for the view (if any) that animation views should be added
// in front of when building an animated transition. It's an error if this
// view is not nil and isn't an immediate subview of the view returned by
// `-animationViewsContainer`
- (UIView*)animationViewsContainerBottomView;

// The frame of the container of the grid, in the `animationViewsContainer`
// coordinates.
- (CGRect)gridContainerFrame;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_GRID_TRANSITION_ANIMATION_LAYOUT_PROVIDING_H_

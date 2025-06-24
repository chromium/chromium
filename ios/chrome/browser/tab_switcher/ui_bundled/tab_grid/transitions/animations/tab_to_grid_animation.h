// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_TO_GRID_ANIMATION_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_TO_GRID_ANIMATION_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_transition_animation.h"

@class TabGridAnimationParameters;

// The animation from the active tab to the tab grid.
//
// This animation handles the transition from an active tab view to a cell
// within the tab grid. It creates a seamless visual effect by taking snapshots
// of the current tab's content, top toolbar, and bottom toolbar. The main
// animated view, representing the tab, is then transformed to match the
// destination grid cell's frame and corner radius. This involves scaling the
// view and applying a mask that animates from the device's corner radius to the
// grid cell's corner radius, while also creating an illusion of the toolbars
// being cropped and fading out. At the same time, the underlying tab grid view,
// initially blurred and scaled down, is animated to its normal state (no blur,
// identity transform) by zooming in from the center of the destination cell.
// Once all animations complete, temporary views and snapshots are removed, and
// the animated view is hidden, revealing the actual grid cell.
@interface TabToGridAnimation : NSObject <TabGridTransitionAnimation>

- (instancetype)initWithAnimationParameters:
    (TabGridAnimationParameters*)animationParameters NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_TO_GRID_ANIMATION_H_

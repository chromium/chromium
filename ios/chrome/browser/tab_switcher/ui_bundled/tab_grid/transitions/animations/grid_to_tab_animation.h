// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_GRID_TO_TAB_ANIMATION_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_GRID_TO_TAB_ANIMATION_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_transition_animation.h"

@class TabGridAnimationParameters;

// The animation from the tab grid to the selected tab.
//
// This animation handles the transition from a selected cell in the tab grid
// back to the full-screen active tab view. It begins by preparing the
// `animatedView` (which will become the full tab) at its final destination
// frame but immediately transforms it to match the `originFrame` of the grid
// cell it's transitioning from. Snapshots of the top and bottom toolbars,
// initially scaled down and partially transparent, are added to this view. A
// snapshot of the tab's content is also added, framed to fit within the
// scaled-down tab. A mask, initially matching the grid cell's corner radius
// and effectively cropping the toolbars, is applied. The animation then
// scales the `animatedView` up to its full size, simultaneously animating the
// mask to the device's corner radius, which reveals the toolbars as they also
// scale to their full size and opacity. The content snapshot animates to its
// full-screen dimensions and then fades out. Concurrently, the underlying tab
// grid view is scaled down and blurred, creating a zoom-out effect centered
// on the originating grid cell. Upon completion, all temporary snapshot
// views, backgrounds, and the blur effect are removed, and the
// `animatedView` is fully revealed in its final state.
@interface GridToTabAnimation : NSObject <TabGridTransitionAnimation>

- (instancetype)initWithAnimationParameters:
    (TabGridAnimationParameters*)animationParameters NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_GRID_TO_TAB_ANIMATION_H_

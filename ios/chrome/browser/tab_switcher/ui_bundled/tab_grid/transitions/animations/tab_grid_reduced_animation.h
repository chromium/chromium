// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_REDUCED_ANIMATION_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_REDUCED_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_transition_animation.h"

// The tab grid's reduce animation. Simply fades in and scales the animated view
// from the center of the screen.
@interface TabGridReducedAnimation : NSObject <TabGridTransitionAnimation>

- (instancetype)initWithAnimatedView:(UIView*)animatedView
                      beingPresented:(BOOL)beingPresented
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_REDUCED_ANIMATION_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_CENTERED_ZOOM_TRANSITION_ANIMATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_CENTERED_ZOOM_TRANSITION_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation.h"

namespace {

// The directions the animation can take.
enum class CenteredZoomTransitionAnimationDirection {
  kContracting,
  kExpanding,
};

}  // namespace

// The animation here creates a simple quick zoom effect -- the tab view
// fades in/out as it expands/contracts. The zoom is not large (75% to 100%)
// and is centered on the view's final center position, so it's not directly
// connected to any tab grid positions.
@interface CenteredZoomTransitionAnimation
    : NSObject <TabGridTransitionAnimation>

// Creates an animation with a `view` to be animated and an animation
// `direction`.
- (instancetype)initWithView:(UIView*)view
                   direction:(CenteredZoomTransitionAnimationDirection)direction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_CENTERED_ZOOM_TRANSITION_ANIMATION_H_

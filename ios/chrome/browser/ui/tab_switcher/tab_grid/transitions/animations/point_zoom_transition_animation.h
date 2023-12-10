// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_POINT_ZOOM_TRANSITION_ANIMATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_POINT_ZOOM_TRANSITION_ANIMATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation.h"

@class TabGridTransitionItem;

// Parameters for the PointZoomTransitionAnimation animation.
struct PointZoomAnimationParameters {
  // The directions the animation can take.
  enum class AnimationDirection {
    kContracting,
    kExpanding,
  };

  AnimationDirection direction;
  // Targeted frame.
  CGRect destinationFrame;
  // Targeted corner radius.
  CGFloat destinationCornerRadius;
};

// The animation here creates a simple, quick zoom effect from the given view to
// the targeted frame. It mainly updates the frame and the corner radius of the
// view.
@interface PointZoomTransitionAnimation : NSObject <TabGridTransitionAnimation>

// Creates an animation with:
// - A `view` to be animated.
// - A set of `animationParameters`.
- (instancetype)initWithView:(UIView*)view
         animationParameters:(PointZoomAnimationParameters)animationParameters
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_ANIMATIONS_POINT_ZOOM_TRANSITION_ANIMATION_H_

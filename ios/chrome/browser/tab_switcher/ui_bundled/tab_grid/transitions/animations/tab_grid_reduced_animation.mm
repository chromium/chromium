// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_reduced_animation.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/prototypes/diamond/utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

// The duration of the reduced animation.
const CGFloat kAnimationDuration = 0.25;

// The corner radius of the animated view when it's scaled down.
const CGFloat kScaledAnimatedViewCornerRadius = 26.0;

// The tab scaling multiplier for the reduced animation.
const CGFloat kScalingMultiplier = 0.75;

}  // namespace

@implementation TabGridReducedAnimation {
  // The view being animated.
  UIView* _animatedView;

  // Whether the tab is being presented, in contrast with it being dismissed.
  BOOL _beingPresented;
}

- (instancetype)initWithAnimatedView:(UIView*)animatedView
                      beingPresented:(BOOL)beingPresented {
  self = [super init];
  if (self) {
    _animatedView = animatedView;
    _beingPresented = beingPresented;
  }
  return self;
}

// The animation here creates a simple quick zoom effect -- the tab view
// fades in/out as it expands/contracts. The zoom is not large (75% to 100%)
// and is centered on the view's final center position, so it's not directly
// connected to any tab grid positions.
- (void)animateWithCompletion:(ProceduralBlock)completion {
  UIView* animatedView = _animatedView;
  CGFloat tabFinalAlpha;
  CGAffineTransform tabFinalTransform;
  CGFloat tabFinalCornerRadius;

  if (_beingPresented) {
    // If presenting, the tab view animates in from 0% opacity, 75% scale
    // transform, and a 26pt corner radius
    animatedView.alpha = 0;
    tabFinalAlpha = 1;
    tabFinalTransform = animatedView.transform;
    animatedView.transform = CGAffineTransformScale(
        tabFinalTransform, kScalingMultiplier, kScalingMultiplier);
    if (IsDiamondPrototypeEnabled()) {
      tabFinalCornerRadius = kDiamondBrowserCornerRadius;
    } else {
      tabFinalCornerRadius = DeviceCornerRadius();
    }
    animatedView.layer.cornerRadius = kScaledAnimatedViewCornerRadius;
  } else {
    // If dismissing, the the tab view animates out to 0% opacity, 75% scale,
    // and 26px corner radius.
    tabFinalAlpha = 0;
    tabFinalTransform = CGAffineTransformScale(
        animatedView.transform, kScalingMultiplier, kScalingMultiplier);
    if (IsDiamondPrototypeEnabled()) {
      animatedView.layer.cornerRadius = kDiamondBrowserCornerRadius;
    } else {
      animatedView.layer.cornerRadius = DeviceCornerRadius();
    }
    tabFinalCornerRadius = kScaledAnimatedViewCornerRadius;
  }

  // Set clipsToBounds on the animating view so its corner radius will look
  // right.
  BOOL oldClipsToBounds = animatedView.clipsToBounds;
  animatedView.clipsToBounds = YES;

  [UIView animateWithDuration:kAnimationDuration
      delay:0.0
      options:UIViewAnimationOptionCurveEaseOut
      animations:^{
        animatedView.alpha = tabFinalAlpha;
        animatedView.transform = tabFinalTransform;
        animatedView.layer.cornerRadius = tabFinalCornerRadius;
      }
      completion:^(BOOL finished) {
        // When presenting the FirstRun ViewController, this can be called with
        // `finished` to NO on official builds. For now, the animation not
        // finishing isn't handled anywhere.
        animatedView.clipsToBounds = oldClipsToBounds;
        if (completion) {
          completion();
        }
      }];
}

@end

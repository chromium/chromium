// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/point_zoom_transition_animation.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

const CGFloat kAnimationDuration = 0.2;

}  // namespace

@implementation PointZoomTransitionAnimation {
  // View that is animated.
  UIView* _animatedView;
  // Default state of the the `clipdsToBounds` property of the `_animatedView`.
  BOOL _animatedViewClipsToBounds;
  // Animation parameters.
  PointZoomAnimationParameters _animationParameters;
}

#pragma mark - Public

- (instancetype)initWithView:(UIView*)view
         animationParameters:(PointZoomAnimationParameters)animationParameters {
  self = [super init];
  if (self) {
    _animatedView = view;
    _animationParameters = animationParameters;
  }
  return self;
}

#pragma mark - TabGridTransitionAnimation

- (void)animateWithCompletion:(ProceduralBlock)completion {
  [self setupAnimationParameters];

  __weak __typeof(self) weakSelf = self;

  ProceduralBlock animationsBlock = ^{
    [weakSelf performAnimation];
  };
  void (^completionBlock)(BOOL) = ^(BOOL finished) {
    if (weakSelf) {
      [weakSelf performAnimationCompletion:completion];
      return;
    }
    if (completion) {
      completion();
    }
  };

  [UIView animateWithDuration:kAnimationDuration
                        delay:0.0
                      options:UIViewAnimationCurveLinear
                   animations:animationsBlock
                   completion:completionBlock];
}

#pragma mark - TabGridTransitionAnimation

// Performs the animation for the `animateWithCompletion` method.
- (void)performAnimation {
  _animatedView.layer.cornerRadius =
      _animationParameters.destinationCornerRadius;
  _animatedView.frame = _animationParameters.destinationFrame;
}

// Performs the animation completion block for the `animateWithCompletion:`
// method.
- (void)performAnimationCompletion:(ProceduralBlock)completion {
  _animatedView.clipsToBounds = _animatedViewClipsToBounds;

  if (completion) {
    completion();
  }
}

// Creates animation parameters.
- (void)setupAnimationParameters {
  _animatedViewClipsToBounds = _animatedView.clipsToBounds;
  _animatedView.clipsToBounds = YES;
  switch (_animationParameters.direction) {
    case PointZoomAnimationParameters::AnimationDirection::kContracting:
      _animatedView.layer.cornerRadius = DeviceCornerRadius();
      break;
    case PointZoomAnimationParameters::AnimationDirection::kExpanding:
      break;
  }
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/centered_zoom_transition_animation.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Animation constants.
const CGFloat kAnimationDuration = 0.25;
const CGFloat kFinalAlpha = 1.0;
const CGFloat kInitialAlpha = 0.0;
const CGFloat kInitialCornerRadius = 26.0;
const CGFloat kInitialTransformScale = 0.75;

// Structure to hold animation parameters.
typedef struct {
  CGFloat alpha;
  CGFloat cornerRadius;
  BOOL clipsToBounds;
  CGAffineTransform transform;
} AnimationParameters;

// Creates animation parameters for the contracted animation.
AnimationParameters CreateContractedAnimationParametersForView(UIView* view) {
  return {kInitialAlpha, kInitialCornerRadius, YES,
          CGAffineTransformScale(view.transform, kInitialTransformScale,
                                 kInitialTransformScale)};
}

// Creates animation parameters for the expanded animation.
AnimationParameters CreateExpandedAnimationParametersForView(UIView* view) {
  return {kFinalAlpha, DeviceCornerRadius(), YES, view.transform};
}

}  // namespace

@implementation CenteredZoomTransitionAnimation {
  UIView* _animatedView;
  CenteredZoomTransitionAnimationDirection _direction;

  AnimationParameters _initialAnimationParameters;
  AnimationParameters _finalAnimationParameters;
}

#pragma mark - Public

- (instancetype)initWithView:(UIView*)view
                   direction:
                       (CenteredZoomTransitionAnimationDirection)direction {
  self = [super init];
  if (self) {
    _animatedView = view;
    _direction = direction;
  }
  return self;
}

- (void)animateWithCompletion:(ProceduralBlock)completion {
  [self setupAnimationParameters];
  [self setInitialAnimationParameters];

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock animationsBlock = ^{
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    strongSelf->_animatedView.alpha =
        strongSelf->_finalAnimationParameters.alpha;
    strongSelf->_animatedView.transform =
        strongSelf->_finalAnimationParameters.transform;
    strongSelf->_animatedView.layer.cornerRadius =
        strongSelf->_finalAnimationParameters.cornerRadius;
  };

  void (^completionBlock)(BOOL) = ^(BOOL finished) {
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    // When presenting the FirstRun ViewController, this can be called with
    // `finished` to NO on official builds. For now, the animation not
    // finishing isn't handled anywhere.
    strongSelf->_animatedView.clipsToBounds =
        strongSelf->_finalAnimationParameters.clipsToBounds;

    if (completion) {
      completion();
    }
  };

  [UIView animateWithDuration:kAnimationDuration
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:animationsBlock
                   completion:completionBlock];
}

#pragma mark - Private

// Creates animation parameters.
- (void)setupAnimationParameters {
  switch (_direction) {
    case CenteredZoomTransitionAnimationDirection::kContracting:
      _initialAnimationParameters =
          CreateExpandedAnimationParametersForView(_animatedView);
      _finalAnimationParameters =
          CreateContractedAnimationParametersForView(_animatedView);
      break;
    case CenteredZoomTransitionAnimationDirection::kExpanding:
      _initialAnimationParameters =
          CreateContractedAnimationParametersForView(_animatedView);
      _finalAnimationParameters =
          CreateExpandedAnimationParametersForView(_animatedView);
      break;
  }

  _initialAnimationParameters.clipsToBounds = YES;
  _finalAnimationParameters.clipsToBounds = _animatedView.clipsToBounds;
}

// Sets initial animation parameters to the view.
- (void)setInitialAnimationParameters {
  _animatedView.alpha = _initialAnimationParameters.alpha;
  _animatedView.transform = _initialAnimationParameters.transform;
  _animatedView.layer.cornerRadius = _initialAnimationParameters.cornerRadius;
  _animatedView.clipsToBounds = _initialAnimationParameters.clipsToBounds;
}

@end

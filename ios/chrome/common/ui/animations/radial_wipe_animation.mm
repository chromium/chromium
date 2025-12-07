// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/animations/radial_wipe_animation.h"

#import "base/check.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Full animation duration;
constexpr CFTimeInterval kAnimationDuration = 1.0;

// Duration for the opacity animation of the color gradient.
constexpr CFTimeInterval kColorGradientOpacityDuration =
    (kAnimationDuration / 3.0);

// Duration for the opacity disappearing animation of the wipe effect animation.
constexpr CFTimeInterval kWipeDisappearingOpacityDuration =
    (kAnimationDuration / 2.0);

// Duration for the opacity disappearing animation of the target view animation.
constexpr CFTimeInterval kTargetViewDisappearingOpacityDuration =
    (kAnimationDuration / 8.0);

// Delay for the start of the disappearing animation of the wipe effect
// animation.
constexpr CFTimeInterval kWipeDisappearingAnimationDelay =
    (kAnimationDuration / 7.0);

// Delay for the start of the target view disappearing animation.
constexpr CFTimeInterval kTargetViewDisappearingAnimationDelay =
    (kAnimationDuration / 10.0);

// Animation start point.
constexpr CGFloat kDefaultAnimationStartPointX = 0.5;
constexpr CGFloat kDefaultAnimationStartPointY = 1.0;

// Disapperating animation radius at the start of the wipe effect animation.
constexpr CGFloat kWipeDisappearinAnimationStartRadius = 0.1;

// Gradient animation radius at the end of the animation.
constexpr CGFloat kColorGradientAnimationEndRadius = 3.2;

// Disapperating animation radius at the end of the wipe effect animation.
constexpr CGFloat kWipeDisappearingAnimationEndRadius = 3.0;

// Target view disapperating animation radius at the end of the animation.
constexpr CGFloat kTargetViewDisappearingAnimationEndRadius = 5.1;

// Returns a white CGColor with opacity `alpha` or `1 - alpha` if `invert`.
UIColor* GetWhiteCGColorRefWithAlpha(CGFloat alpha, bool invert) {
  return [UIColor colorWithWhite:1.0 alpha:invert ? 1 - alpha : alpha];
}

// Returns an animated disappearing gradient the size of `frame` for a target
// view. Callers are expected to add the gradient to the view hierarchy.
// `start_time_in_superlayer` is the animation's start time in the time space of
// the superlayer. Superlayer refers to the parent layer where gradient created
// by this function will be added to.
CAGradientLayer* GetAnimatedTargetViewDisappearingGradient(
    CGRect frame,
    NSTimeInterval start_time_in_superlayer,
    bool invert_alpha,
    CGPoint start_point) {
  CAGradientLayer* gradient_layer = [CAGradientLayer layer];
  gradient_layer.type = kCAGradientLayerRadial;

  // Start with mostly opaque and ease disappearing while the circle expands.
  gradient_layer.colors = @[
    (id)GetWhiteCGColorRefWithAlpha(0.9, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(0.94, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(0.97, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(0.98, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(1.0, invert_alpha).CGColor,
  ];

  // The circle begins mostly opaque, and comes into view by making the view
  // disappear as it grows.
  gradient_layer.startPoint = start_point;
  gradient_layer.endPoint = start_point;

  CGFloat frame_size = MAX(frame.size.width, frame.size.height);
  gradient_layer.frame =
      CGRectMake(CGPointZero.x, CGPointZero.y, frame_size, frame_size);

  NSTimeInterval start_time =
      [gradient_layer convertTime:start_time_in_superlayer fromLayer:nil];

  // Circle expanding animation. We'll use the end point to expand the circle
  // past the size of the frame so we end up with a fully transparent view at
  // the end.
  CABasicAnimation* expand_animation =
      [CABasicAnimation animationWithKeyPath:@"endPoint"];
  expand_animation.beginTime =
      start_time + kTargetViewDisappearingAnimationDelay;
  expand_animation.duration =
      kAnimationDuration - kTargetViewDisappearingAnimationDelay;
  expand_animation.fromValue =
      [NSValue valueWithCGPoint:gradient_layer.endPoint];
  expand_animation.toValue = [NSValue
      valueWithCGPoint:CGPointMake(
                           start_point.x +
                               kTargetViewDisappearingAnimationEndRadius,
                           start_point.y -
                               kTargetViewDisappearingAnimationEndRadius)];

  // Prolong the end state of the animation, so the view continues to be fully
  // transparent.
  expand_animation.fillMode = kCAFillModeForwards;
  expand_animation.removedOnCompletion = NO;

  [gradient_layer addAnimation:expand_animation forKey:@"endPoint"];

  // Opacity animation. The circle shouldn't be fully visible from the start,
  // but come into view while it's expanding.
  CABasicAnimation* opacity_animation =
      [CABasicAnimation animationWithKeyPath:@"colors"];
  // The opacity animation should be shorter than the main one since the circle
  // should be fully visible before it stops growing.
  opacity_animation.beginTime =
      start_time + kTargetViewDisappearingAnimationDelay;
  opacity_animation.duration = kTargetViewDisappearingOpacityDuration;
  opacity_animation.fromValue = gradient_layer.colors;
  opacity_animation.toValue = @[
    (id)GetWhiteCGColorRefWithAlpha(0.0, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(0.0, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(0.5, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(1.0, invert_alpha).CGColor,
    (id)GetWhiteCGColorRefWithAlpha(1.0, invert_alpha).CGColor,
  ];

  // Prolong the end state of the animation, so the view continues to be
  // transparent.
  opacity_animation.fillMode = kCAFillModeForwards;
  opacity_animation.removedOnCompletion = NO;

  [gradient_layer addAnimation:opacity_animation forKey:@"colors"];

  return gradient_layer;
}

// Returns an animated disappearing gradient the size of `frame` for the wipe
// gradient. Callers are expected to add the gradient to the view hierarchy.
// `start_time_in_superlayer` is the animation's start time in the time space of
// the superlayer. Superlayer refers to the parent layer where gradient created
// by this function will be added to.
CAGradientLayer* GetAnimatedWipeDisappearingGradient(
    CGRect frame,
    NSTimeInterval start_time_in_superlayer,
    CGPoint start_point) {
  CAGradientLayer* gradient_layer = [CAGradientLayer layer];
  gradient_layer.type = kCAGradientLayerRadial;

  // The circle begins mostly opaque, and comes into view by making the view
  // disappear as it grows.
  gradient_layer.colors = @[
    (id)[UIColor colorWithWhite:1.0 alpha:0.95].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:0.97].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:0.99].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:1.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:1.0].CGColor,
  ];

  // Start already with a small circle. Since the circle begins mostly opaque,
  // there isn't a harsh transition.
  gradient_layer.startPoint = start_point;
  gradient_layer.endPoint =
      CGPointMake(start_point.x + kWipeDisappearinAnimationStartRadius,
                  start_point.y - kWipeDisappearinAnimationStartRadius);

  CGFloat frame_size = MAX(frame.size.width, frame.size.height);
  gradient_layer.frame =
      CGRectMake(CGPointZero.x, CGPointZero.y, frame_size, frame_size);

  NSTimeInterval start_time =
      [gradient_layer convertTime:start_time_in_superlayer fromLayer:nil];

  // Circle expanding animation. We'll use the end point to expand the circle
  // past the size of the frame so we end up with a fully transparent view at
  // the end.
  CABasicAnimation* expand_animation =
      [CABasicAnimation animationWithKeyPath:@"endPoint"];
  expand_animation.beginTime = start_time + kWipeDisappearingAnimationDelay;
  expand_animation.duration =
      kAnimationDuration - kWipeDisappearingAnimationDelay;
  expand_animation.fromValue =
      [NSValue valueWithCGPoint:gradient_layer.endPoint];
  expand_animation.toValue = [NSValue
      valueWithCGPoint:CGPointMake(
                           start_point.x + kWipeDisappearingAnimationEndRadius,
                           start_point.y -
                               kWipeDisappearingAnimationEndRadius)];

  // Prolong the end state of the animation, so the view continues to be fully
  // transparent.
  expand_animation.fillMode = kCAFillModeForwards;
  expand_animation.removedOnCompletion = NO;

  [gradient_layer addAnimation:expand_animation forKey:@"endPoint"];

  // Opacity animation. The circle shouldn't be fully visible from the start,
  // but come into view while it's expanding.
  CABasicAnimation* opacity_animation =
      [CABasicAnimation animationWithKeyPath:@"colors"];
  // The opacity animation should be shorter than the main one since the circle
  // should be fully visible before it stops growing.
  opacity_animation.beginTime = start_time + kWipeDisappearingAnimationDelay;
  opacity_animation.duration = kWipeDisappearingOpacityDuration;
  opacity_animation.fromValue = gradient_layer.colors;
  opacity_animation.toValue = @[
    (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:0.5].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:1.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:1.0].CGColor,
  ];

  // Prolong the end state of the animation, so the view continues to be
  // transparent.
  opacity_animation.fillMode = kCAFillModeForwards;
  opacity_animation.removedOnCompletion = NO;

  [gradient_layer addAnimation:opacity_animation forKey:@"colors"];

  return gradient_layer;
}

// Returns the animated gradient that creates a "wipe" effect the size of
// `frame`. Callers are expected to add the gradient to the view hierarchy.
// `start_time_in_superlayer` is the animation's start time in the time space of
// the superlayer. Superlayer refers to the parent layer where gradient created
// by this function will be added to.
CAGradientLayer* GetAnimatedWipeEffect(CGRect frame,
                                       NSTimeInterval duration,
                                       NSTimeInterval start_time_in_superlayer,
                                       CGPoint start_point) {
  CAGradientLayer* gradient_layer = [CAGradientLayer layer];
  gradient_layer.type = kCAGradientLayerRadial;
  gradient_layer.colors = @[
    (id)[[UIColor colorNamed:kStaticBlueColor] colorWithAlphaComponent:0.6]
        .CGColor,
    (id)[[UIColor colorNamed:kStaticBlueColor] colorWithAlphaComponent:0.4]
        .CGColor,
    (id)[[UIColor colorNamed:kStaticBlueColor] colorWithAlphaComponent:0.0]
        .CGColor,
  ];

  // The gradient shouldn't be visible at the start.
  gradient_layer.startPoint = start_point;
  gradient_layer.endPoint = start_point;

  CGFloat frame_size = MAX(frame.size.width, frame.size.height);
  gradient_layer.frame =
      CGRectMake(CGPointZero.x, CGPointZero.y, frame_size, frame_size);

  NSTimeInterval start_time =
      [gradient_layer convertTime:start_time_in_superlayer fromLayer:nil];

  // Expand circle animation.  We'll use the end point to expand the circle past
  // the size of the frame so we end up with a fully colored view at the end.
  CABasicAnimation* end_point_animation =
      [CABasicAnimation animationWithKeyPath:@"endPoint"];
  end_point_animation.beginTime = start_time;
  end_point_animation.duration = duration;
  end_point_animation.fromValue =
      [NSValue valueWithCGPoint:gradient_layer.endPoint];
  end_point_animation.toValue = [NSValue
      valueWithCGPoint:CGPointMake(
                           start_point.x + kColorGradientAnimationEndRadius,
                           start_point.y + kColorGradientAnimationEndRadius)];

  [gradient_layer addAnimation:end_point_animation forKey:@"endPoint"];

  // Opacity animation. The circle shouldn't be fully visible from the start,
  // but come into view while it's expanding.
  CABasicAnimation* colors_animation =
      [CABasicAnimation animationWithKeyPath:@"colors"];
  // The opacity animation should be shorter than the main one since the circle
  // should be mostly opaque before it stops growing.
  colors_animation.beginTime = start_time;
  colors_animation.duration = kColorGradientOpacityDuration;
  colors_animation.byValue = gradient_layer.colors;
  colors_animation.toValue = @[
    (id)[[UIColor colorNamed:kStaticBlueColor] colorWithAlphaComponent:0.7]
        .CGColor,
    (id)[[UIColor colorNamed:kStaticBlueColor] colorWithAlphaComponent:0.45]
        .CGColor,
    (id)[[UIColor colorNamed:kStaticBlueColor] colorWithAlphaComponent:0.0]
        .CGColor,
  ];

  // Prolong the end state of the animation.
  // In reality, it will be transparent due to `inner_gradient_layer` being
  // applied to `gradient_layer`.
  colors_animation.fillMode = kCAFillModeForwards;
  colors_animation.removedOnCompletion = NO;

  [gradient_layer addAnimation:colors_animation forKey:@"colors"];

  // Add the gradient to animate the disappearing of the blue circle shown by
  // `gradient_layer`.
  CAGradientLayer* inner_gradient_layer = GetAnimatedWipeDisappearingGradient(
      frame, [gradient_layer convertTime:start_time fromLayer:nil],
      start_point);
  gradient_layer.mask = inner_gradient_layer;

  return gradient_layer;
}
}  // namespace

@implementation RadialWipeAnimation {
  UIView* _window;
  NSArray<UIView*>* _targetViews;
  CAGradientLayer* _gradientLayer;
}

#pragma mark - Public

- (instancetype)initWithWindow:(UIView*)window
                   targetViews:(NSArray<UIView*>*)targetViews {
  self = [super init];
  if (self) {
    _window = window;
    _targetViews = targetViews;
    _startPoint =
        CGPointMake(kDefaultAnimationStartPointX, kDefaultAnimationStartPointY);
    _type = RadialWipeAnimationType::kHideTarget;
  }
  return self;
}

- (void)animateWithCompletion:(ProceduralBlock)completion {
  CHECK(!_window.userInteractionEnabled);
  _window.userInteractionEnabled = NO;

  [CATransaction begin];
  [CATransaction
      setAnimationTimingFunction:MaterialTimingFunction(MaterialCurveEaseIn)];
  [CATransaction setAnimationDuration:kAnimationDuration];

  __weak RadialWipeAnimation* weakSelf = self;
  [CATransaction setCompletionBlock:^{
    [weakSelf onAnimationCompleted];

    if (completion) {
      completion();
    }
  }];

  CFTimeInterval mediaTime = CACurrentMediaTime();

  [self addWipeEffectAnimationWithMediaTime:mediaTime];
  [self addTargetViewDisappearingAnimationWithMediaTime:mediaTime];

  [CATransaction commit];
}

#pragma mark - Private

// Adds the "wipe" effect animation to `window` with `mediaTime`.
- (void)addWipeEffectAnimationWithMediaTime:(CFTimeInterval)mediaTime {
  _gradientLayer = GetAnimatedWipeEffect(
      _window.bounds, kAnimationDuration,
      [_window.layer convertTime:mediaTime fromLayer:nil], self.startPoint);
  // The animation should happen centered on the window.
  _gradientLayer.position = CGPointMake(_window.bounds.size.width / 2.0,
                                        _window.bounds.size.height / 2.0);
  [_window.layer addSublayer:_gradientLayer];
}

// Adds the disappearing animation to all views in `_targetViews` with
// `mediaTime`.
- (void)addTargetViewDisappearingAnimationWithMediaTime:
    (CFTimeInterval)mediaTime {
  for (UIView* view : _targetViews) {
    bool invertMaskAlpha =
        (self.type == RadialWipeAnimationType::kRevealTarget);
    // Using the generalized helper function.
    CAGradientLayer* viewGradientLayer =
        GetAnimatedTargetViewDisappearingGradient(
            _window.bounds, [view.layer convertTime:mediaTime fromLayer:nil],
            invertMaskAlpha, self.startPoint);

    // Get the frame of the view in the window's coordinate system.
    CGRect viewInWindow = [_window convertRect:view.frame
                                      fromView:view.superview];
    // Adjust the gradient layer's position based on the offset of the view
    // relative to the center of the window.
    viewGradientLayer.position =
        CGPointMake(_window.bounds.size.width / 2.0 - viewInWindow.origin.x,
                    _window.bounds.size.height / 2.0 - viewInWindow.origin.y);
    view.layer.mask = viewGradientLayer;
  }
}

// Cleans up the view hierarchy after the animation has run.
- (void)onAnimationCompleted {
  // Remove the main gradient layer after the animation has completed.
  [_gradientLayer removeFromSuperlayer];
  _gradientLayer = nil;

  // Remove the gradient layer mask from each target view and set the final
  // visibility state.
  for (UIView* view : _targetViews) {
    view.hidden = (self.type == RadialWipeAnimationType::kHideTarget);
    view.layer.mask = nil;
  }
}

@end

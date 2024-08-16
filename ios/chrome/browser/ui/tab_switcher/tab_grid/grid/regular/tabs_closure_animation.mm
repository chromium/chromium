// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/tabs_closure_animation.h"

#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Tab closure full animation duration;
constexpr CFTimeInterval kAnimationDuration = 1.3;

// Duration for the opacity disappearing animation.
constexpr CFTimeInterval kDisappearingOpacityDuration =
    kAnimationDuration / 3.0;

// Delay for the start of the grid cell disappearing animation.
constexpr CFTimeInterval kGridCellDisappearingAnimationDelay = 0.0;

// Delay for the start of the disappearing animation of the wipe effect
// animation.
constexpr CFTimeInterval kWipeDisappearingAnimationDelay =
    kDisappearingOpacityDuration - (kAnimationDuration / 10.0);

// Tab closure animation start point.
constexpr CGFloat kAnimationStartPointX = 0.5;
constexpr CGFloat kAnimationStartPointY = 1.0;

// Disapperating animation radius at the start of the animation.
constexpr CGFloat kDisappearinAnimationStartRadius = 0.1;

// Gradient animation radius at the end of the animation.
constexpr CGFloat kColorGradientAnimationEndRadius = 2.5;

// Grid cell disapperating animation radius at the end of the animation.
constexpr CGFloat kGridCellDisappearingAnimationEndRadius = 2.8;

// Disapperating animation radius at the end of the wipe effect animation.
constexpr CGFloat kWipeDisappearingAnimationEndRadius = 1.8;

// Returns an animated disappearing gradient the size of `frame`. Callers are
// expected to add the gradient to the view hierarchy.
CAGradientLayer* GetAnimatedDisappearingGradient(CGRect frame,
                                                 CGFloat end_radius,
                                                 NSTimeInterval duration,
                                                 NSTimeInterval delay) {
  CAGradientLayer* gradient_layer = [CAGradientLayer layer];
  gradient_layer.type = kCAGradientLayerRadial;

  // Start with everything fully opaque and ease disapering while the circle
  // expands.
  gradient_layer.colors = @[
    (id)[UIColor colorWithWhite:1.0 alpha:1.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:1.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:1.0].CGColor,
  ];

  // Start already with a small circle. Since, the circle begins fully opaque,
  // there isn't a harsh transition.
  gradient_layer.startPoint =
      CGPointMake(kAnimationStartPointX, kAnimationStartPointY);
  gradient_layer.endPoint =
      CGPointMake(kAnimationStartPointX + kDisappearinAnimationStartRadius,
                  kAnimationStartPointY - kDisappearinAnimationStartRadius);

  CGFloat frame_size = MAX(frame.size.width, frame.size.height);
  gradient_layer.frame =
      CGRectMake(CGPointZero.x, CGPointZero.y, frame_size, frame_size);

  NSTimeInterval start_time = [gradient_layer convertTime:CACurrentMediaTime()
                                                fromLayer:nil];

  // Circle expanding animation. We'll use the end point to expand the circle
  // past the size of the frame so we end up with a fully transparent view at
  // the end.
  CABasicAnimation* expandAnimation =
      [CABasicAnimation animationWithKeyPath:@"endPoint"];
  expandAnimation.duration = duration - delay;
  expandAnimation.beginTime = start_time + delay;
  expandAnimation.fromValue =
      [NSValue valueWithCGPoint:gradient_layer.endPoint];
  expandAnimation.toValue = [NSValue
      valueWithCGPoint:CGPointMake(kAnimationStartPointX + end_radius,
                                   kAnimationStartPointY - end_radius)];

  // Prolong the end state of the animation, so the view continues to be fully
  // transparent.
  expandAnimation.fillMode = kCAFillModeForwards;
  expandAnimation.removedOnCompletion = NO;

  [gradient_layer addAnimation:expandAnimation forKey:@"endPoint"];

  // Opacity animation. The circle shouldn't be fully visible from the start,
  // but come into view while it's expanding.
  CABasicAnimation* opacity_animation =
      [CABasicAnimation animationWithKeyPath:@"colors"];
  // The opacity animation should be shorter than the main one since the circle
  // should be fully transparent before it stops growing.
  opacity_animation.duration = kDisappearingOpacityDuration;
  opacity_animation.beginTime = start_time + delay;
  opacity_animation.fromValue = gradient_layer.colors;
  opacity_animation.toValue = @[
    (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor,
    (id)[UIColor colorWithWhite:1.0 alpha:0.8].CGColor,
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
CAGradientLayer* GetAnimatedWipeEffect(CGRect frame, NSTimeInterval duration) {
  CAGradientLayer* gradient_layer = [CAGradientLayer layer];
  gradient_layer.type = kCAGradientLayerRadial;
  gradient_layer.colors = @[
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.0].CGColor,
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.0].CGColor,
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.0].CGColor,
  ];

  // The gradient shouldn't be visible at the start.
  gradient_layer.startPoint =
      CGPointMake(kAnimationStartPointX, kAnimationStartPointY);
  gradient_layer.endPoint =
      CGPointMake(kAnimationStartPointX, kAnimationStartPointY);
  CGFloat frame_size = MAX(frame.size.width, frame.size.height);
  gradient_layer.frame =
      CGRectMake(CGPointZero.x, CGPointZero.y, frame_size, frame_size);

  NSTimeInterval startTime = [gradient_layer convertTime:CACurrentMediaTime()
                                               fromLayer:nil];

  // Expand circle animation.  We'll use the end point to expand the circle past
  // the size of the frame so we end up with a fully colored view at the end.
  CABasicAnimation* end_point_animation =
      [CABasicAnimation animationWithKeyPath:@"endPoint"];
  end_point_animation.duration = duration;
  end_point_animation.beginTime = startTime;
  end_point_animation.fromValue =
      [NSValue valueWithCGPoint:gradient_layer.endPoint];
  end_point_animation.toValue = [NSValue
      valueWithCGPoint:CGPointMake(kAnimationStartPointX +
                                       kColorGradientAnimationEndRadius,
                                   kAnimationStartPointY +
                                       kColorGradientAnimationEndRadius)];

  [gradient_layer addAnimation:end_point_animation forKey:@"endPoint"];

  // Opacity animation. The circle shouldn't be fully visible from the start,
  // but come into view while it's expanding.
  CABasicAnimation* colors_animation =
      [CABasicAnimation animationWithKeyPath:@"colors"];
  // The opacity animation should be shorter than the main one since the circle
  // should be fully opaque before it stops growing.
  colors_animation.beginTime = startTime;
  colors_animation.duration = kDisappearingOpacityDuration;
  colors_animation.fromValue = gradient_layer.colors;
  colors_animation.byValue = @[
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.1].CGColor,
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.05].CGColor,
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.0].CGColor,
  ];
  colors_animation.toValue = @[
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.7].CGColor,
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.35].CGColor,
    (id)[[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.0].CGColor,
  ];

  // Prolong the end state of the animation, so the window continues to be blue.
  // In reality, it will be transparent due to `inner_gradient_layer` being
  // applied to `gradient_layer`.
  colors_animation.fillMode = kCAFillModeForwards;
  colors_animation.removedOnCompletion = NO;

  [gradient_layer addAnimation:colors_animation forKey:@"colors"];

  // Add the gradient to animate the disapering of the blue circle shown by
  // `gradient_layer`.
  CAGradientLayer* inner_gradient_layer = GetAnimatedDisappearingGradient(
      frame, kWipeDisappearingAnimationEndRadius, duration,
      kWipeDisappearingAnimationDelay);
  gradient_layer.mask = inner_gradient_layer;

  return gradient_layer;
}
}  // namespace

@implementation TabsClosureAnimation {
  UIView* _window;
  NSArray<UIView*>* _gridCells;
  CAGradientLayer* _gradientLayer;
}

#pragma mark - Public

- (instancetype)initWithWindow:(UIView*)window
                     gridCells:(NSArray<UIView*>*)gridCells {
  self = [super init];
  if (self) {
    _window = window;
    _gridCells = gridCells;
  }
  return self;
}

- (void)animateWithCompletion:(ProceduralBlock)completion {
  [CATransaction begin];
  [CATransaction
      setAnimationTimingFunction:MaterialTimingFunction(MaterialCurveEaseIn)];
  [CATransaction setAnimationDuration:kAnimationDuration];

  __weak TabsClosureAnimation* weakSelf = self;
  UIView* window = _window;
  window.userInteractionEnabled = NO;
  [CATransaction setCompletionBlock:^{
    window.userInteractionEnabled = YES;
    [weakSelf onAnimationCompletedWithCompletionBlock:completion];
  }];

  [self addWipeEffectAnimation];
  [self addGridCellDisapperingAnimation];

  [CATransaction commit];
}

#pragma mark - Private

// Adds the "wipe" effect animation to `window`.
- (void)addWipeEffectAnimation {
  _gradientLayer = GetAnimatedWipeEffect(_window.frame, kAnimationDuration);
  // The grid view is scrollable. The animation should happen on what is visible
  // in the window not in the middle of the grid view which might not even be
  // visible.
  _gradientLayer.position = _window.center;
  [_window.layer addSublayer:_gradientLayer];
}

// Adds the disappering animation to all views in `_gridCells`.
- (void)addGridCellDisapperingAnimation {
  for (UIView* cell : _gridCells) {
    CAGradientLayer* gridCellGradientLayer = GetAnimatedDisappearingGradient(
        _window.frame, kGridCellDisappearingAnimationEndRadius,
        kAnimationDuration, kGridCellDisappearingAnimationDelay);

    // Get position of the cell on the tab grid's coordinate system. The
    // `gridCellGradientLayer` position coordinates are in the cell's coordinate
    // system. As such, we need to adjust the `gridCellGradientLayer`'s position
    // with the position of the cell in its superview, the grid view.
    CGRect cellInTabGrid = [_window convertRect:cell.frame
                                       fromView:cell.superview];
    gridCellGradientLayer.position =
        CGPointMake(_window.center.x - cellInTabGrid.origin.x,
                    _window.center.y - cellInTabGrid.origin.y);
    cell.layer.mask = gridCellGradientLayer;
  }
}

// Cleans up the view hierarchy after the animation has run by removing
// unnecessary layers.
- (void)onAnimationCompletedWithCompletionBlock:(ProceduralBlock)completion {
  // Remove the main gradient layer after the animation has completed.
  [_gradientLayer removeFromSuperlayer];
  _gradientLayer = nil;

  // Remove the gradient layer in each grid cell mask in favor of hiding the
  // view.
  for (UIView* cell : _gridCells) {
    cell.hidden = YES;
    cell.layer.mask = nil;
  }

  if (completion) {
    completion();
  }
}

@end

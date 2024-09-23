// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_view.h"

#import <cmath>
#import <numbers>

#import "base/check.h"
#import "base/numerics/math_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

enum class SwipeType { CHANGE_TABS, NAVIGATION };

typedef struct {
  CGFloat min;
  CGFloat max;
} FloatRange;

CGFloat MapValueToRange(FloatRange from, FloatRange to, CGFloat value) {
  DCHECK(from.min < from.max);
  if (value <= from.min)
    return to.min;
  if (value >= from.max)
    return to.max;
  const CGFloat fromDst = from.max - from.min;
  const CGFloat toDst = to.max - to.min;
  return to.min + ((value - from.min) / fromDst) * toDst;
}

// The portion of the screen width a swipe must travel after which a navigation
// should be initiated.
const CGFloat kSwipeThreshold = 0.53;

// Convert the velocity (which is measured in points per second) to points per
// `kSwipeVelocityFraction` of a second.
const CGFloat kSwipeVelocityFraction = 0.1;

// Distance after which the arrow should animate in.
const CGFloat kArrowThreshold = 32;

// Duration of the snapping animation when the selection bubble animates.
const CGFloat kSelectionSnappingAnimationDuration = 0.2;

// Size of the selection circle.
const CGFloat kSelectionSize = 64.0;

// Start scale of the selection circle.
const CGFloat kSelectionDownScale = 0.1875;

// The final scale of the selection bubble when the threshold is met.
const CGFloat kSelectionAnimationScale = 26;

// The duration of the animations played when the threshold is met.
const NSTimeInterval kSelectionAnimationDuration = 0.5;

UIColor* SelectionCircleColor() {
  return [UIColor colorNamed:kTextfieldBackgroundColor];
}
}

@interface SideSwipeNavigationView () {
 @private
  // Has the current swipe gone past the point where the action would trigger?
  // Will be reset to NO if it recedes before that point (ie, not a latch).
  BOOL _thresholdTriggered;

  // The back or forward sprite image.
  UIImageView* _arrowView;

  // The selection bubble.
  CAShapeLayer* _selectionCircleLayer;

  // If `NO` this is an edge gesture and navigation isn't possible. Don't show
  // arrows and bubbles and don't allow navigate.
  BOOL _canNavigate;
}
// Returns a newly allocated and configured selection circle shape.
- (CAShapeLayer*)newSelectionCircleLayer;
// Pushes the touch towards the edge because it's difficult to touch the very
// edge of the screen (touches tend to sit near x ~ 4).
- (CGPoint)adjustPointToEdge:(CGPoint)point;
@end

@implementation SideSwipeNavigationView

@synthesize targetView = _targetView;

- (instancetype)initWithFrame:(CGRect)frame
                withDirection:(UISwipeGestureRecognizerDirection)direction
                  canNavigate:(BOOL)canNavigate
                        image:(UIImage*)image {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];

    _canNavigate = canNavigate;
    if (canNavigate) {
      image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      const CGRect imageSize = CGRectMake(0, 0, 24, 24);
      _arrowView = [[UIImageView alloc] initWithImage:image];
      _arrowView.tintColor = [UIColor colorNamed:kToolbarButtonColor];
      _selectionCircleLayer = [self newSelectionCircleLayer];
      [_arrowView setFrame:imageSize];
    }

    CGFloat borderWidth = ui::AlignValueToUpperPixel(kToolbarSeparatorHeight);

    CGRect borderFrame = CGRectMake(0, 0, borderWidth, self.frame.size.height);
    UIView* border = [[UIView alloc] initWithFrame:borderFrame];
    border.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
    [self addSubview:border];
    if (direction == UISwipeGestureRecognizerDirectionRight) {
      borderFrame.origin.x = frame.size.width - borderWidth;
      [border setFrame:borderFrame];
      [border setAutoresizingMask:UIViewAutoresizingFlexibleLeftMargin];
    } else {
      [border setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
    }

    [self.layer addSublayer:_selectionCircleLayer];
    [self setClipsToBounds:YES];
    [self addSubview:_arrowView];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
        UITraitUserInterfaceIdiom.self, UITraitUserInterfaceStyle.self,
        UITraitDisplayGamut.self, UITraitAccessibilityContrast.self,
        UITraitUserInterfaceLevel.self
      ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateSelectionCircleColorOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (CGPoint)adjustPointToEdge:(CGPoint)currentPoint {
  CGFloat width = CGRectGetWidth(self.targetView.bounds);
  CGFloat half = floor(width / 2);
  CGFloat padding = floor(std::abs(currentPoint.x - half) / half);

  // Push towards the edges.
  if (currentPoint.x > half)
    currentPoint.x += padding;
  else
    currentPoint.x -= padding;

  // But don't go past the edges.
  if (currentPoint.x < 0)
    currentPoint.x = 0;
  else if (currentPoint.x > width)
    currentPoint.x = width;

  return currentPoint;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateSelectionCircleColorOnTraitChange:previousTraitCollection];
}
#endif

- (void)updateFrameAndAnimateContents:(CGFloat)distance
                         forDirection:
                             (UISwipeGestureRecognizerDirection)direction {
  CGFloat width = CGRectGetWidth(self.targetView.bounds);

  // Immediately set frame size.
  CGRect frame = self.frame;
  if (direction == UISwipeGestureRecognizerDirectionRight) {
    frame.size.width = self.targetView.frame.origin.x;
    frame.origin.x = 0;
  } else {
    frame.origin.x = self.targetView.frame.origin.x + width;
    frame.size.width = width - frame.origin.x;
  }
  [self setFrame:frame];

  // Move `selectionCircleLayer_` without animations.
  CGRect bounds = self.bounds;
  CGPoint center = CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
  [_arrowView setCenter:AlignPointToPixel(center)];
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [_selectionCircleLayer setPosition:center];
  [CATransaction commit];

  CGFloat rotationStart = -CGFloat(std::numbers::pi) / 2;
  CGFloat rotationEnd = 0;
  if (direction == UISwipeGestureRecognizerDirectionLeft) {
    rotationStart = CGFloat(std::numbers::pi) * 1.5;
    rotationEnd = CGFloat(std::numbers::pi);
  }
  CGAffineTransform rotation = CGAffineTransformMakeRotation(MapValueToRange(
      {0, kArrowThreshold}, {rotationStart, rotationEnd}, distance));
  CGFloat scale = MapValueToRange({0, kArrowThreshold}, {0, 1}, distance);
  [_arrowView setTransform:CGAffineTransformScale(rotation, scale, scale)];

  // Animate selection bubbles dpending on distance.
  __weak SideSwipeNavigationView* weakSelf = self;
  [UIView animateWithDuration:kSelectionSnappingAnimationDuration
                   animations:^{
                     [weakSelf animateSelectionBubblesByDistance:distance
                                                           width:width];
                   }
                   completion:nil];
}

- (void)animateSelectionBubblesByDistance:(CGFloat)distance
                                    width:(CGFloat)width {
  if (distance < (width * kSwipeThreshold)) {
    // Scale selection down.
    _selectionCircleLayer.transform =
        CATransform3DMakeScale(kSelectionDownScale, kSelectionDownScale, 1);
    _selectionCircleLayer.opacity = 0;
    [_arrowView setAlpha:MapValueToRange({0, 64}, {0, 1}, distance)];
    _thresholdTriggered = NO;
  } else {
    _selectionCircleLayer.transform = CATransform3DMakeScale(1, 1, 1);
    _selectionCircleLayer.opacity = 1;
    [_arrowView setAlpha:1];
    // Trigger a small haptic blip when exceeding the
    // threshold and mark such that only one blip gets
    // triggered.
    if (!_thresholdTriggered) {
      TriggerHapticFeedbackForSelectionChange();
      _thresholdTriggered = YES;
    }
  }
}

- (void)explodeSelection:(void (^)(void))block {
  __weak SideSwipeNavigationView* weakSelf = self;
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    [weakSelf handleCATransactionComplete:block];
  }];

  CAMediaTimingFunction* timing =
      MaterialTimingFunction(MaterialCurveEaseInOut);
  CABasicAnimation* scaleAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform"];
  scaleAnimation.fromValue =
      [NSValue valueWithCATransform3D:CATransform3DIdentity];
  scaleAnimation.toValue =
      [NSValue valueWithCATransform3D:CATransform3DMakeScale(
                                          kSelectionAnimationScale,
                                          kSelectionAnimationScale, 1)];
  scaleAnimation.timingFunction = timing;
  scaleAnimation.duration = kSelectionAnimationDuration;
  scaleAnimation.fillMode = kCAFillModeForwards;
  scaleAnimation.removedOnCompletion = NO;
  [_selectionCircleLayer addAnimation:scaleAnimation forKey:@"transform"];

  CABasicAnimation* opacityAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  opacityAnimation.fromValue = @(_selectionCircleLayer.opacity);
  opacityAnimation.toValue = @(1);
  opacityAnimation.timingFunction = timing;
  opacityAnimation.duration = kSelectionAnimationDuration;
  opacityAnimation.fillMode = kCAFillModeForwards;
  opacityAnimation.removedOnCompletion = NO;
  [_selectionCircleLayer addAnimation:opacityAnimation forKey:@"opacity"];

  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  positionAnimation.fromValue =
      [NSValue valueWithCGPoint:_selectionCircleLayer.position];

  CGPoint finalPosition = CGPointMake([self.targetView superview].center.x,
                                      _selectionCircleLayer.position.y);
  positionAnimation.toValue = [NSValue valueWithCGPoint:finalPosition];
  positionAnimation.timingFunction = timing;
  positionAnimation.duration = kSelectionAnimationDuration;
  positionAnimation.fillMode = kCAFillModeForwards;
  positionAnimation.removedOnCompletion = NO;
  [_selectionCircleLayer addAnimation:positionAnimation forKey:@"position"];
  [CATransaction commit];

  [_arrowView setAlpha:1];
  [UIView animateWithDuration:kSelectionAnimationDuration
                   animations:^{
                     [weakSelf setArrowViewAlpha:0];
                   }];
}

- (void)handleCATransactionComplete:(void (^)(void))block {
  // Note that the animations below may complete at slightly different times
  // resulting in frame(s) between animation completion and the transaction's
  // completion handler that show the original state. To avoid this flicker,
  // the animations use a fillMode forward and are not removed until the
  // transaction completion handler is executed.
  [_selectionCircleLayer removeAnimationForKey:@"opacity"];
  [_selectionCircleLayer removeAnimationForKey:@"transform"];
  [_selectionCircleLayer setOpacity:0];
  [_arrowView setAlpha:0];
  self.backgroundColor = SelectionCircleColor();
  block();
}

- (void)setArrowViewAlpha:(CGFloat)alpha {
  [_arrowView setAlpha:alpha];
}

- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture
     onOverThresholdCompletion:(void (^)(void))onOverThresholdCompletion
    onUnderThresholdCompletion:(void (^)(void))onUnderThresholdCompletion {
  CGPoint currentPoint = [gesture locationInView:gesture.view];
  CGPoint velocityPoint = [gesture velocityInView:gesture.view];
  currentPoint.x -= gesture.swipeOffset;

  // Push point to edge.
  currentPoint = [self adjustPointToEdge:currentPoint];

  CGFloat distance = currentPoint.x;
  // The snap back animation is 0.1 seconds, so convert the velocity distance
  // to where the `x` position would in .1 seconds.
  CGFloat velocityOffset = velocityPoint.x * kSwipeVelocityFraction;
  CGFloat width = CGRectGetWidth(self.targetView.bounds);
  if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
    distance = width - distance;
    velocityOffset = -velocityOffset;
  }

  if (!_canNavigate) {
    // shrink distance a bit to make the drag feel springier.
    distance /= 3;
  }

  CGRect frame = self.targetView.frame;
  if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
    frame.origin.x = -distance;
  } else {
    frame.origin.x = distance;
  }
  self.targetView.frame = frame;

  [self updateFrameAndAnimateContents:distance forDirection:gesture.direction];

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled ||
      gesture.state == UIGestureRecognizerStateFailed) {
    CGFloat threshold = width * kSwipeThreshold;
    CGFloat finalDistance = distance + velocityOffset;
    // Ensure the actual distance traveled has met the minimum arrow threshold
    // and that the distance including expected velocity is over `threshold`.
    if (distance > kArrowThreshold && finalDistance > threshold &&
        _canNavigate && gesture.state == UIGestureRecognizerStateEnded) {
      // Speed up the animation for higher velocity swipes.
      NSTimeInterval animationTime = MapValueToRange(
          {threshold, width},
          {kSelectionAnimationDuration, kSelectionAnimationDuration / 2},
          finalDistance);
      [self performNavigationAnimationWithDirection:gesture.direction
                                           duration:animationTime
                                  completionHandler:onOverThresholdCompletion];
    } else {
      [self animateTargetViewCompleted:NO
                         withDirection:gesture.direction
                          withDuration:0.1];
      onUnderThresholdCompletion();
    }
    _thresholdTriggered = NO;
  }
}

- (void)animateHorizontalPanWithDirection:
            (UISwipeGestureRecognizerDirection)direction
                        completionHandler:(void (^)(void))completion {
  CGFloat width = CGRectGetWidth(self.targetView.bounds);
  CGFloat distance = width * kSwipeThreshold;
  CGRect frame = self.targetView.frame;
  if (direction == UISwipeGestureRecognizerDirectionLeft) {
    frame.origin.x = -distance;
  } else {
    frame.origin.x = distance;
  }
  self.targetView.frame = frame;

  [self updateFrameAndAnimateContents:distance forDirection:direction];
  if (_canNavigate) {
    [self performNavigationAnimationWithDirection:direction
                                         duration:kSelectionAnimationDuration
                                completionHandler:completion];
  } else {
    [self animateTargetViewCompleted:NO
                       withDirection:direction
                        withDuration:kSelectionAnimationDuration];
  }
}

- (void)animateTargetViewCompleted:(BOOL)completed
                     withDirection:(UISwipeGestureRecognizerDirection)direction
                      withDuration:(NSTimeInterval)duration {
  __weak SideSwipeNavigationView* weakSelf = self;
  NSTimeInterval cleanUpDelay =
      completed ? kSelectionAnimationDuration - duration : 0;
  [UIView animateWithDuration:duration
      animations:^{
        [weakSelf handleTargetViewAnimationWithCompleted:completed
                                           withDirection:direction];
      }
      completion:^(BOOL finished) {
        // Give the other animations time to complete.
        dispatch_after(
            dispatch_time(DISPATCH_TIME_NOW, cleanUpDelay * NSEC_PER_SEC),
            dispatch_get_main_queue(), ^{
              [weakSelf handleTargetViewAnimationCompletion];
            });
      }];
}

- (void)handleTargetViewAnimationWithCompleted:(BOOL)completed
                                 withDirection:
                                     (UISwipeGestureRecognizerDirection)
                                         direction {
  CGRect targetFrame = self.targetView.frame;
  CGRect frame = self.frame;
  CGFloat width = CGRectGetWidth(self.targetView.bounds);
  // Animate self.targetFrame to the side if completed and to the center if
  // not. Animate self.view to the center if completed or to the size if not.
  if (completed) {
    frame.origin.x = 0;
    frame.size.width = width;
    self.frame = frame;
    targetFrame.origin.x =
        direction == UISwipeGestureRecognizerDirectionRight ? width : -width;
    self.targetView.frame = targetFrame;
  } else {
    targetFrame.origin.x = 0;
    self.targetView.frame = targetFrame;
    frame.origin.x =
        direction == UISwipeGestureRecognizerDirectionLeft ? width : 0;
    frame.size.width = 0;
    self.frame = frame;
  }
  CGRect bounds = self.bounds;
  CGPoint center = CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
  [_arrowView setCenter:AlignPointToPixel(center)];
}

// Animate navigation with the duration `animationTime` and execute completion
// handler afterwards.
- (void)performNavigationAnimationWithDirection:
            (UISwipeGestureRecognizerDirection)direction
                                       duration:(NSTimeInterval)animationTime
                              completionHandler:(void (^)(void))block {
  TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleMedium);
  [self animateTargetViewCompleted:YES
                     withDirection:direction
                      withDuration:animationTime];
  [self explodeSelection:block];
}

- (void)handleTargetViewAnimationCompletion {
  // Reset target frame.
  CGRect frame = self.targetView.frame;
  frame.origin.x = 0;
  self.targetView.frame = frame;
  [self removeFromSuperview];
}

- (CAShapeLayer*)newSelectionCircleLayer {
  const CGRect bounds = CGRectMake(0, 0, kSelectionSize, kSelectionSize);
  CAShapeLayer* selectionCircleLayer = [[CAShapeLayer alloc] init];
  selectionCircleLayer.bounds = bounds;
  selectionCircleLayer.backgroundColor = UIColor.clearColor.CGColor;
  UIColor* resolvedColor = [SelectionCircleColor()
      resolvedColorWithTraitCollection:self.traitCollection];
  selectionCircleLayer.fillColor = resolvedColor.CGColor;
  selectionCircleLayer.opacity = 0;
  selectionCircleLayer.transform =
      CATransform3DMakeScale(kSelectionDownScale, kSelectionDownScale, 1);
  selectionCircleLayer.path =
      [[UIBezierPath bezierPathWithOvalInRect:bounds] CGPath];

  return selectionCircleLayer;
}

// Update the `_selectionCircleLayer` property's fill color if the change
// navigation view's traits caused the appearance to change colors.
- (void)updateSelectionCircleColorOnTraitChange:
    (UITraitCollection*)previousTraitCollection {
  if (![self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    return;
  }

  _selectionCircleLayer.fillColor = SelectionCircleColor().CGColor;
}

@end

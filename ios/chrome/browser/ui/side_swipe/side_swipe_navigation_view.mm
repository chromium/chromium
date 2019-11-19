// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/side_swipe_navigation_view.h"

#include "base/logging.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/numerics/math_constants.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/material_timing.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
// |kSwipeVelocityFraction| of a second.
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
const CGFloat kSelectionAnimationDuration = 0.5;

UIColor* const kPageBackgroundColor = [UIColor colorNamed:kBackgroundColor];
UIColor* const kSelectionCircleColor =
    [UIColor colorNamed:kTextfieldBackgroundColor];
UIColor* const kArrowColor = [UIColor colorNamed:kToolbarButtonColor];
}

@interface SideSwipeNavigationView () {
 @private
  // Has the current swipe gone past the point where the action would trigger?
  // Will be reset to NO if it recedes before that point (ie, not a latch).
  BOOL thresholdTriggered_;

  // The back or forward sprite image.
  UIImageView* arrowView_;

  // The selection bubble.
  CAShapeLayer* selectionCircleLayer_;

  // If |NO| this is an edge gesture and navigation isn't possible. Don't show
  // arrows and bubbles and don't allow navigate.
  BOOL canNavigate_;
}
// Returns a newly allocated and configured selection circle shape.
- (CAShapeLayer*)newSelectionCircleLayer;
// Pushes the touch towards the edge because it's difficult to touch the very
// edge of the screen (touches tend to sit near x ~ 4).
- (CGPoint)adjustPointToEdge:(CGPoint)point;
@end

@implementation SideSwipeNavigationView

@synthesize targetView = targetView_;

- (instancetype)initWithFrame:(CGRect)frame
                withDirection:(UISwipeGestureRecognizerDirection)direction
                  canNavigate:(BOOL)canNavigate
                        image:(UIImage*)image {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = kPageBackgroundColor;

    canNavigate_ = canNavigate;
    if (canNavigate) {
      image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      const CGRect imageSize = CGRectMake(0, 0, 24, 24);
      arrowView_ = [[UIImageView alloc] initWithImage:image];
      arrowView_.tintColor = kArrowColor;
      selectionCircleLayer_ = [self newSelectionCircleLayer];
      [arrowView_ setFrame:imageSize];
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

    [self.layer addSublayer:selectionCircleLayer_];
    [self setClipsToBounds:YES];
    [self addSubview:arrowView_];
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

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (@available(iOS 13, *)) {
    if ([self.traitCollection
            hasDifferentColorAppearanceComparedToTraitCollection:
                previousTraitCollection]) {
      selectionCircleLayer_.fillColor = kSelectionCircleColor.CGColor;
    }
  }
}

- (void)updateFrameAndAnimateContents:(CGFloat)distance
                           forGesture:(SideSwipeGestureRecognizer*)gesture {
  CGFloat width = CGRectGetWidth(self.targetView.bounds);

  // Immediately set frame size.
  CGRect frame = self.frame;
  if (gesture.direction == UISwipeGestureRecognizerDirectionRight) {
    frame.size.width = self.targetView.frame.origin.x;
    frame.origin.x = 0;
  } else {
    frame.origin.x = self.targetView.frame.origin.x + width;
    frame.size.width = width - frame.origin.x;
  }
  [self setFrame:frame];

  // Move |selectionCircleLayer_| without animations.
  CGRect bounds = self.bounds;
  CGPoint center = CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
  [arrowView_ setCenter:AlignPointToPixel(center)];
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [selectionCircleLayer_ setPosition:center];
  [CATransaction commit];

  CGFloat rotationStart = -CGFloat(base::kPiDouble) / 2;
  CGFloat rotationEnd = 0;
  if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
    rotationStart = CGFloat(base::kPiDouble) * 1.5;
    rotationEnd = CGFloat(base::kPiDouble);
  }
  CGAffineTransform rotation = CGAffineTransformMakeRotation(MapValueToRange(
      {0, kArrowThreshold}, {rotationStart, rotationEnd}, distance));
  CGFloat scale = MapValueToRange({0, kArrowThreshold}, {0, 1}, distance);
  [arrowView_ setTransform:CGAffineTransformScale(rotation, scale, scale)];

  // Animate selection bubbles dpending on distance.
  [UIView beginAnimations:@"transform" context:NULL];
  [UIView setAnimationDuration:kSelectionSnappingAnimationDuration];
  if (distance < (width * kSwipeThreshold)) {
    // Scale selection down.
    selectionCircleLayer_.transform =
        CATransform3DMakeScale(kSelectionDownScale, kSelectionDownScale, 1);
    selectionCircleLayer_.opacity = 0;
    [arrowView_ setAlpha:MapValueToRange({0, 64}, {0, 1}, distance)];
    thresholdTriggered_ = NO;
  } else {
    selectionCircleLayer_.transform = CATransform3DMakeScale(1, 1, 1);
    selectionCircleLayer_.opacity = 1;
    [arrowView_ setAlpha:1];
    // Trigger a small haptic blip when exceeding the threshold and mark
    // such that only one blip gets triggered.
    if (!thresholdTriggered_) {
      TriggerHapticFeedbackForSelectionChange();
      thresholdTriggered_ = YES;
    }
  }
  [UIView commitAnimations];
}

- (void)explodeSelection:(void (^)(void))block {
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    // Note that the animations below may complete at slightly different times
    // resulting in frame(s) between animation completion and the transaction's
    // completion handler that show the original state. To avoid this flicker,
    // the animations use a fillMode forward and are not removed until the
    // transaction completion handler is executed.
    [selectionCircleLayer_ removeAnimationForKey:@"opacity"];
    [selectionCircleLayer_ removeAnimationForKey:@"transform"];
    [selectionCircleLayer_ setOpacity:0];
    [arrowView_ setAlpha:0];
    self.backgroundColor = kSelectionCircleColor;
    block();

  }];

  CAMediaTimingFunction* timing =
      ios::material::TimingFunction(ios::material::CurveEaseInOut);
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
  [selectionCircleLayer_ addAnimation:scaleAnimation forKey:@"transform"];

  CABasicAnimation* opacityAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  opacityAnimation.fromValue = @(selectionCircleLayer_.opacity);
  opacityAnimation.toValue = @(1);
  opacityAnimation.timingFunction = timing;
  opacityAnimation.duration = kSelectionAnimationDuration;
  opacityAnimation.fillMode = kCAFillModeForwards;
  opacityAnimation.removedOnCompletion = NO;
  [selectionCircleLayer_ addAnimation:opacityAnimation forKey:@"opacity"];

  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  positionAnimation.fromValue =
      [NSValue valueWithCGPoint:selectionCircleLayer_.position];

  CGPoint finalPosition = CGPointMake([self.targetView superview].center.x,
                                      selectionCircleLayer_.position.y);
  positionAnimation.toValue = [NSValue valueWithCGPoint:finalPosition];
  positionAnimation.timingFunction = timing;
  positionAnimation.duration = kSelectionAnimationDuration;
  positionAnimation.fillMode = kCAFillModeForwards;
  positionAnimation.removedOnCompletion = NO;
  [selectionCircleLayer_ addAnimation:positionAnimation forKey:@"position"];
  [CATransaction commit];

  [arrowView_ setAlpha:1];
  [UIView animateWithDuration:kSelectionAnimationDuration
                   animations:^{
                     [arrowView_ setAlpha:0];
                   }];
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
  // to where the |x| position would in .1 seconds.
  CGFloat velocityOffset = velocityPoint.x * kSwipeVelocityFraction;
  CGFloat width = CGRectGetWidth(self.targetView.bounds);
  if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
    distance = width - distance;
    velocityOffset = -velocityOffset;
  }

  if (!canNavigate_) {
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

  [self updateFrameAndAnimateContents:distance forGesture:gesture];

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled ||
      gesture.state == UIGestureRecognizerStateFailed) {
    CGFloat threshold = width * kSwipeThreshold;
    CGFloat finalDistance = distance + velocityOffset;
    // Ensure the actual distance traveled has met the minimum arrow threshold
    // and that the distance including expected velocity is over |threshold|.
    if (distance > kArrowThreshold && finalDistance > threshold &&
        canNavigate_ && gesture.state == UIGestureRecognizerStateEnded) {
      TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleMedium);

      // Speed up the animation for higher velocity swipes.
      CGFloat animationTime = MapValueToRange(
          {threshold, width},
          {kSelectionAnimationDuration, kSelectionAnimationDuration / 2},
          finalDistance);
      [self animateTargetViewCompleted:YES
                         withDirection:gesture.direction
                          withDuration:animationTime];
      [self explodeSelection:onOverThresholdCompletion];
      if (IsSwipingForward(gesture.direction)) {
        base::RecordAction(base::UserMetricsAction(
            "MobileEdgeSwipeNavigationForwardCompleted"));
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileEdgeSwipeNavigationBackCompleted"));
      }
    } else {
      [self animateTargetViewCompleted:NO
                         withDirection:gesture.direction
                          withDuration:0.1];
      onUnderThresholdCompletion();
      if (IsSwipingForward(gesture.direction)) {
        base::RecordAction(base::UserMetricsAction(
            "MobileEdgeSwipeNavigationForwardCancelled"));
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileEdgeSwipeNavigationBackCancelled"));
      }
    }
    thresholdTriggered_ = NO;
  }
}

- (void)animateTargetViewCompleted:(BOOL)completed
                     withDirection:(UISwipeGestureRecognizerDirection)direction
                      withDuration:(CGFloat)duration {
  void (^animationBlock)(void) = ^{
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
    [arrowView_ setCenter:AlignPointToPixel(center)];
  };
  CGFloat cleanUpDelay = completed ? kSelectionAnimationDuration - duration : 0;
  [UIView animateWithDuration:duration
                   animations:animationBlock
                   completion:^(BOOL finished) {
                     // Give the other animations time to complete.
                     dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                                  cleanUpDelay * NSEC_PER_SEC),
                                    dispatch_get_main_queue(), ^{
                                      // Reset target frame.
                                      CGRect frame = self.targetView.frame;
                                      frame.origin.x = 0;
                                      self.targetView.frame = frame;
                                      [self removeFromSuperview];
                                    });
                   }];
}

- (CAShapeLayer*)newSelectionCircleLayer {
  const CGRect bounds = CGRectMake(0, 0, kSelectionSize, kSelectionSize);
  CAShapeLayer* selectionCircleLayer = [[CAShapeLayer alloc] init];
  selectionCircleLayer.bounds = bounds;
  selectionCircleLayer.backgroundColor = UIColor.clearColor.CGColor;
  if (@available(iOS 13, *)) {
    UIColor* resolvedColor = [kSelectionCircleColor
        resolvedColorWithTraitCollection:self.traitCollection];
    selectionCircleLayer.fillColor = resolvedColor.CGColor;
  } else {
    selectionCircleLayer.fillColor = kSelectionCircleColor.CGColor;
  }
  selectionCircleLayer.opacity = 0;
  selectionCircleLayer.transform =
      CATransform3DMakeScale(kSelectionDownScale, kSelectionDownScale, 1);
  selectionCircleLayer.path =
      [[UIBezierPath bezierPathWithOvalInRect:bounds] CGPath];

  return selectionCircleLayer;
}

@end

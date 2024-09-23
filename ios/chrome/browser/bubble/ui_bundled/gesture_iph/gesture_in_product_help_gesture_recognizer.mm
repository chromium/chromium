// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_gesture_recognizer.h"

#import <cmath>

#import "base/i18n/rtl.h"
#import "base/numerics/angle_conversions.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"

namespace {

/// Maxmimum angle to the expected direction that should be recognized as a
/// swipe. For example, a swipe-right tilted 30 degrees downwards can still be
/// recognized as a "swipe down", because it's only deviated from "down" 60
/// degrees; however a swipe-right tilted 20 degrees downwards would not be
/// recognized as a "swipe down".
const CGFloat kMaxSwipeAngle = 65;
/// The minimum distance between touches for a swipe to begin.
const CGFloat kDefaultMinSwipeThreshold = 4;
// Swipe starting distance from edge.
const CGFloat kSwipeEdge = 20;

/// Returns the opposite direction to `direction`.
UISwipeGestureRecognizerDirection GetOppositeDirection(
    UISwipeGestureRecognizerDirection direction) {
  switch (direction) {
    case UISwipeGestureRecognizerDirectionUp:
      return UISwipeGestureRecognizerDirectionDown;
    case UISwipeGestureRecognizerDirectionDown:
      return UISwipeGestureRecognizerDirectionUp;
    case UISwipeGestureRecognizerDirectionLeft:
      return UISwipeGestureRecognizerDirectionRight;
    case UISwipeGestureRecognizerDirectionRight:
    default:
      return UISwipeGestureRecognizerDirectionLeft;
  }
}

}  // namespace

@implementation GestureInProductHelpGestureRecognizer {
  /// Expected swipe direction; passed through the initializer.
  UISwipeGestureRecognizerDirection _expectedSwipeDirection;
  /// Starting point of swipe.
  CGPoint _startPoint;
}

- (instancetype)initWithExpectedSwipeDirection:
                    (UISwipeGestureRecognizerDirection)direction
                                        target:(id)target
                                        action:(SEL)action {
  self = [super initWithTarget:target action:action];
  if (self) {
    _expectedSwipeDirection = direction;
    _bidirectional = NO;
    _edgeSwipe = NO;
    _startPoint = CGPointZero;
    _actualSwipeDirection = 0;
    [self setMaximumNumberOfTouches:1];
  }
  return self;
}

/// To quickly avoid interference with other gesture recognizers, fail
/// immediately if the touches aren't at the edge of the touched view.
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];

  // Don't interrupt gestures in progress.
  if (self.state != UIGestureRecognizerStatePossible) {
    return;
  }

  UITouch* touch = [[event allTouches] anyObject];
  CGPoint location = [touch locationInView:self.view];
  _startPoint = location;
}

- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
  // Revert to normal pan gesture recognizer characteristics after state began.
  if (self.state != UIGestureRecognizerStatePossible) {
    [super touchesMoved:touches withEvent:event];
    return;
  }

  // Only one touch.
  if ([[event allTouches] count] > 1) {
    self.state = UIGestureRecognizerStateFailed;
    return;
  }

  // Avoid recognizing swipe at an angle greater than `kMaxSwipeAngle`.
  UITouch* touch = [[event allTouches] anyObject];
  CGPoint currentPoint = [touch locationInView:self.view];
  CGFloat dx = currentPoint.x - _startPoint.x;
  CGFloat dy = currentPoint.y - _startPoint.y;
  CGFloat degrees = 0;
  CGFloat distance = 0;
  switch (_expectedSwipeDirection) {
    case UISwipeGestureRecognizerDirectionUp:
      distance = std::fabs(dy);
      degrees = base::RadToDeg(std::atan2(std::fabs(dx), -dy));
      break;
    case UISwipeGestureRecognizerDirectionDown:
      distance = std::fabs(dy);
      degrees = base::RadToDeg(std::atan2(std::fabs(dx), dy));
      break;
    case UISwipeGestureRecognizerDirectionLeft:
      distance = std::fabs(dx);
      degrees = base::RadToDeg(std::atan2(std::fabs(dy), -dx));
      break;
    case UISwipeGestureRecognizerDirectionRight:
      distance = std::fabs(dx);
      degrees = base::RadToDeg(std::atan2(std::fabs(dy), dx));
      break;
  }
  if (distance < kDefaultMinSwipeThreshold) {
    return;
  }

  UIGestureRecognizerState state = UIGestureRecognizerStateFailed;
  if (0 <= degrees && degrees < kMaxSwipeAngle) {
    state = UIGestureRecognizerStateBegan;
    _actualSwipeDirection = _expectedSwipeDirection;
  } else if (self.bidirectional &&
             (180 - kMaxSwipeAngle < degrees && degrees <= 180)) {
    state = UIGestureRecognizerStateBegan;
    _actualSwipeDirection = GetOppositeDirection(_expectedSwipeDirection);
  }

  // If `[self isEdgeSwipe]`, determine whether the touch has started
  // from the edge.
  if (state != UIGestureRecognizerStateFailed && [self isEdgeSwipe]) {
    CGFloat startPointFromEdge = CGFLOAT_MAX;
    switch (_actualSwipeDirection) {
      case UISwipeGestureRecognizerDirectionUp:
        startPointFromEdge = CGRectGetHeight(self.view.bounds) - _startPoint.y;
        break;
      case UISwipeGestureRecognizerDirectionDown:
        startPointFromEdge = _startPoint.y;
        break;
      case UISwipeGestureRecognizerDirectionLeft:
        startPointFromEdge = CGRectGetWidth(self.view.bounds) - _startPoint.x;
        break;
      case UISwipeGestureRecognizerDirectionRight:
        startPointFromEdge = _startPoint.x;
        break;
    }
    if (startPointFromEdge > kSwipeEdge) {
      state = UIGestureRecognizerStateFailed;
    }
  }
  self.state = state;
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  _startPoint = CGPointZero;
  [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  _startPoint = CGPointZero;
  [super touchesCancelled:touches withEvent:event];
}

@end

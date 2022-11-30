// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/force_touch_long_press_gesture_recognizer.h"

#import <UIKit/UIGestureRecognizerSubclass.h>
#import "base/cxx17_backports.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ForceTouchLongPressGestureRecognizer

@synthesize forceThreshold = _forceThreshold;

- (void)setForceThreshold:(CGFloat)forceThreshold {
  _forceThreshold = base::clamp<CGFloat>(forceThreshold, 0, 1);
}

#pragma mark - UIGestureRecognizerSubclass

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  [self touchesChanged:touches toPhase:UITouchPhaseBegan];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesMoved:touches withEvent:event];
  [self touchesChanged:touches toPhase:UITouchPhaseMoved];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  [self touchesChanged:touches toPhase:UITouchPhaseEnded];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  [self touchesChanged:touches toPhase:UITouchPhaseCancelled];
}

#pragma mark - Private

- (void)touchesChanged:(NSSet<UITouch*>*)touches
               toPhase:(UITouchPhase)touchPhase {
  UITouch* touch = [touches anyObject];
  if (!touch) {
    self.state = UIGestureRecognizerStateFailed;
    return;
  }

  switch (touchPhase) {
    case UITouchPhaseCancelled:
      self.state = UIGestureRecognizerStateCancelled;
      break;
    case UITouchPhaseEnded:
      self.state = UIGestureRecognizerStateEnded;
      break;
    case UITouchPhaseBegan:
    // Falls through.
    case UITouchPhaseMoved:
      if (self.state == UIGestureRecognizerStatePossible) {
        // Gesture hasn't begun yet.
        if (touch.force / touch.maximumPossibleForce >= self.forceThreshold)
          self.state = UIGestureRecognizerStateBegan;
      } else {
        self.state = UIGestureRecognizerStateChanged;
      }
      break;
    default:
      break;
  }
}

@end

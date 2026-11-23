// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/disabled_buttons_tap_gesture_recognizer.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"

namespace {

// Minimum alpha for a view to be considered interactive by UIKit hit-testing.
constexpr CGFloat kMinimumInteractiveAlpha = 0.01;

// Returns whether `point_in_ancestor` (in `ancestor`'s coordinate space) lands
// inside `button` while `button` is disabled and every view from `button` up to
// `ancestor` is visible, user-interaction-enabled, and contains the point.
bool IsPointInVisibleDisabledButton(CGPoint point_in_ancestor,
                                    UIButton* button,
                                    UIView* ancestor) {
  if (!button || button.enabled) {
    return false;
  }
  UIView* view = button;
  for (; view && view != ancestor; view = view.superview) {
    CGPoint point_in_view = [ancestor convertPoint:point_in_ancestor
                                            toView:view];
    if (view.hidden || !view.userInteractionEnabled ||
        view.alpha <= kMinimumInteractiveAlpha ||
        ![view pointInside:point_in_view withEvent:nil]) {
      return false;
    }
  }
  return view == ancestor;
}

}  // namespace

@interface DisabledButtonsTapGestureRecognizer () <UIGestureRecognizerDelegate>
@end

@implementation DisabledButtonsTapGestureRecognizer {
  NSHashTable<UIButton*>* _buttons;
}

- (instancetype)initWithTarget:(id)target
                        action:(SEL)action
                       buttons:(NSArray<UIButton*>*)buttons {
  self = [super initWithTarget:target action:action];
  if (self) {
    _buttons = [NSHashTable weakObjectsHashTable];
    for (UIButton* button in buttons) {
      [_buttons addObject:button];
    }
    self.cancelsTouchesInView = NO;
    // Act as our own delegate so touches are filtered before tracking begins
    // without requiring owning view controllers to conform to
    // `UIGestureRecognizerDelegate`.
    [super setDelegate:self];
  }
  return self;
}

- (void)setDelegate:(id<UIGestureRecognizerDelegate>)delegate {
  CHECK(!delegate || delegate == self);
  [super setDelegate:delegate];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  UIView* ancestor = self.view;
  UIView* hitView = touch.view;
  if (!ancestor || !hitView) {
    return NO;
  }
  // Never intercept touches that hit-tested onto (or inside) an enabled
  // control.
  for (UIView* view = hitView; view && view != ancestor;
       view = view.superview) {
    UIControl* hitControl = base::apple::ObjCCast<UIControl>(view);
    if (hitControl && hitControl.enabled) {
      return NO;
    }
  }
  CGPoint pointInAncestor = [touch locationInView:ancestor];
  for (UIButton* button in _buttons) {
    // A disabled `UIButton` normally remains the hit-test view (ignoring
    // `UIControl` touch events while still receiving touches), or a custom
    // container/control may resolve `hitView` to an ancestor. Accept either
    // relationship while rejecting unrelated overlapping sibling branches.
    if (!([button isDescendantOfView:hitView] ||
          [hitView isDescendantOfView:button])) {
      continue;
    }
    if (IsPointInVisibleDisabledButton(pointInAncestor, button, ancestor)) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  UIView* ancestor = self.view;
  if (!ancestor) {
    return NO;
  }
  // Re-verify at recognition time so dragging off the button within tap slop
  // or a state change before finger lift does not misfire.
  CGPoint pointInAncestor = [gestureRecognizer locationInView:ancestor];
  for (UIButton* button in _buttons) {
    if (IsPointInVisibleDisabledButton(pointInAncestor, button, ancestor)) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRequireFailureOfGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Let pan gestures (such as toolbar side-swipes) take precedence over
  // disabled-button taps; on genuine taps, pan recognizers fail synchronously
  // in `touchesEnded:` without adding delay.
  return [otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]];
}

@end

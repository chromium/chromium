// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_snapshot_navigation_view.h"

#import <cmath>
#import <numbers>

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_gesture_recognizer.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// When the swipe gesture reaches this value, the navigation is initiated.
const CGFloat kSwipeThreshold = 0.53;

// The duration of the animations played when the swipe threshold is met.
const NSTimeInterval kOverThresholdAnimationDuration = 0.3;

// The duration of the animations played when the swipe threshold is not met.
const NSTimeInterval kUnderThresholdAnimationDuration = 0.1;

// The navigation delay duration.
const NSTimeInterval kNavigationDelay = 0.2;

}  // namespace

@implementation SideSwipeSnapshotNavigationView

// Synthesized from protocol.
@synthesize targetView = _targetView;

- (instancetype)initWithFrame:(CGRect)frame snapshot:(UIImage*)snapshotImage {
  self = [super initWithFrame:frame];
  if (self) {
    UIImageView* snapshot = [[UIImageView alloc] initWithImage:snapshotImage];
    snapshot.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:snapshot];
    AddSameConstraints(self, snapshot);
  }
  return self;
}

- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture
     onOverThresholdCompletion:(void (^)(void))onOverThresholdCompletion
    onUnderThresholdCompletion:(void (^)(void))onUnderThresholdCompletion {
  CGPoint currentPoint = [gesture locationInView:gesture.view.superview];
  CGFloat width = CGRectGetWidth(self.targetView.bounds);
  CGFloat distance = gesture.direction == UISwipeGestureRecognizerDirectionLeft
                         ? (width - currentPoint.x + gesture.swipeOffset)
                         : currentPoint.x - gesture.swipeOffset;
  CGRect frame = self.targetView.frame;
  if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
    frame.origin.x = -distance;
  } else {
    frame.origin.x = distance;
  }
  self.targetView.frame = frame;

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled ||
      gesture.state == UIGestureRecognizerStateFailed) {
    CGFloat threshold = width * kSwipeThreshold;
    // Ensure the actual distance traveled has met the minimum arrow threshold
    // and that the distance including expected velocity is over `threshold`.
    if (distance > threshold &&
        gesture.state == UIGestureRecognizerStateEnded) {
      [self performNavigationAnimationWithDirection:gesture.direction
                                           duration:
                                               kOverThresholdAnimationDuration
                                  completionHandler:onOverThresholdCompletion];
    } else {
      [self animateTargetViewCompleted:NO
                         withDirection:gesture.direction
                          withDuration:kUnderThresholdAnimationDuration
                     completionHandler:onUnderThresholdCompletion];
    }
  }
}

- (void)animateHorizontalPanWithDirection:
            (UISwipeGestureRecognizerDirection)direction
                        completionHandler:(void (^)(void))completion {
  [self performNavigationAnimationWithDirection:direction
                                       duration:kOverThresholdAnimationDuration
                              completionHandler:completion];
}

- (void)moveTargetViewOffscreenInDirection:
    (UISwipeGestureRecognizerDirection)direction {
  CGFloat width = CGRectGetWidth(self.targetView.bounds);
  // Position the target view's frame offscreen.
  CGRect targetFrame = self.targetView.frame;
  targetFrame.origin.x =
      direction == UISwipeGestureRecognizerDirectionLeft ? width : -width;
  self.targetView.frame = targetFrame;
}

- (void)moveTargetViewOnScreenWithAnimation {
  [UIView animateWithDuration:kOverThresholdAnimationDuration
      animations:^{
        [self resetTargetViewFrame];
      }
      completion:^(BOOL finished) {
        [self handleTargetViewAnimationCompletion];
      }];
}

#pragma mark - Private

// Animates navigation with the duration `animationTime` and execute completion
// handler afterwards.
- (void)performNavigationAnimationWithDirection:
            (UISwipeGestureRecognizerDirection)direction
                                       duration:(NSTimeInterval)animationTime
                              completionHandler:(void (^)(void))block {
  TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleMedium);
  [self animateTargetViewCompleted:YES
                     withDirection:direction
                      withDuration:animationTime
                 completionHandler:block];
}

// Animates the targetView to slide to the side.
- (void)animateTargetViewCompleted:(BOOL)completed
                     withDirection:(UISwipeGestureRecognizerDirection)direction
                      withDuration:(NSTimeInterval)duration
                 completionHandler:(void (^)(void))block {
  __weak SideSwipeSnapshotNavigationView* weakSelf = self;
  [UIView animateWithDuration:duration
      animations:^{
        [weakSelf handleTargetViewAnimationWithCompleted:completed
                                           withDirection:direction];
      }
      completion:^(BOOL finished) {
        block();
        // To avoid flickering, the target frame reset is delayed until after
        // the web view navigation completes.
        dispatch_after(
            dispatch_time(DISPATCH_TIME_NOW, kNavigationDelay * NSEC_PER_SEC),
            dispatch_get_main_queue(), ^{
              [weakSelf handleTargetViewAnimationCompletion];
            });
      }];
}

// Moves the targetFrame to the side if completed, resets it to fullscreen
// otherwise.
- (void)handleTargetViewAnimationWithCompleted:(BOOL)completed
                                 withDirection:
                                     (UISwipeGestureRecognizerDirection)
                                         direction {
  CGRect targetFrame = self.targetView.frame;
  CGFloat width = CGRectGetWidth(targetFrame);

  if (completed) {
    targetFrame.origin.x =
        direction == UISwipeGestureRecognizerDirectionRight ? width : -width;
  } else {
    targetFrame.origin.x = 0;
  }

  self.targetView.frame = targetFrame;
}

// Resets the target frame.
- (void)resetTargetViewFrame {
  CGRect frame = self.targetView.frame;
  frame.origin.x = 0;
  self.targetView.frame = frame;
}

// Resets the target frame and removes the view.
- (void)handleTargetViewAnimationCompletion {
  [self resetTargetViewFrame];
  [self removeFromSuperview];
}

@end

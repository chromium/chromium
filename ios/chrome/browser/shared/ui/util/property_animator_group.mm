// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/property_animator_group.h"

#import "base/check.h"

@implementation PropertyAnimatorGroup {
  NSMutableArray<UIViewPropertyAnimator*>* _animators;
}

@synthesize animators = _animators;

- (instancetype)init {
  if ((self = [super init])) {
    _animators = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)addAnimator:(UIViewPropertyAnimator*)animator {
  DCHECK(self.animators.count == 0 ||
         animator.duration == self.animators[0].duration);
  DCHECK(self.animators.count == 0 ||
         animator.delay == self.animators[0].delay);
  [_animators addObject:animator];
}

#pragma mark UIViewAnimating

- (void)startAnimation {
  for (UIViewPropertyAnimator* animator in self.animators) {
    [animator startAnimation];
  }
}

- (void)startAnimationAfterDelay:(NSTimeInterval)delay {
  for (UIViewPropertyAnimator* animator in self.animators) {
    [animator startAnimationAfterDelay:delay];
  }
}

- (void)pauseAnimation {
  for (UIViewPropertyAnimator* animator in self.animators) {
    [animator pauseAnimation];
  }
}

- (void)stopAnimation:(BOOL)withoutFinishing {
  for (UIViewPropertyAnimator* animator in self.animators) {
    [animator stopAnimation:withoutFinishing];
  }
}

- (void)finishAnimationAtPosition:(UIViewAnimatingPosition)finalPosition {
  for (UIViewPropertyAnimator* animator in self.animators) {
    [animator finishAnimationAtPosition:finalPosition];
  }
}

- (CGFloat)fractionComplete {
  return self.animators[0].fractionComplete;
}

- (void)setFractionComplete:(CGFloat)fractionComplete {
  for (UIViewPropertyAnimator* animator in self.animators) {
    animator.fractionComplete = fractionComplete;
  }
}

- (BOOL)isReversed {
  return self.animators[0].reversed;
}

- (void)setReversed:(BOOL)reversed {
  for (UIViewPropertyAnimator* animator in self.animators) {
    animator.reversed = reversed;
  }
}

- (UIViewAnimatingState)state {
  return self.animators[0].state;
}

- (BOOL)isRunning {
  return self.animators[0].running;
}

#pragma mark UIViewImplicitlyAnimating

- (void)addAnimations:(void (^)())animation {
  [self.animators[0] addAnimations:animation];
}

- (void)addAnimations:(void (^)())animation delayFactor:(CGFloat)delayFactor {
  [self.animators[0] addAnimations:animation delayFactor:delayFactor];
}

- (void)addCompletion:(void (^)(UIViewAnimatingPosition))completion {
  [self.animators[0] addCompletion:completion];
}

- (void)continueAnimationWithTimingParameters:
            (id<UITimingCurveProvider>)parameters
                               durationFactor:(CGFloat)durationFactor {
  [self.animators[0] continueAnimationWithTimingParameters:parameters
                                            durationFactor:durationFactor];
}

@end

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/optional_property_animator.h"

@interface OptionalPropertyAnimator ()

// Redefine property as readwrite.
@property(nonatomic, readwrite) BOOL hasAnimations;

@end

@implementation OptionalPropertyAnimator
@synthesize hasAnimations = _hasAnimations;

#pragma mark - UIViewPropertyAnimator

- (instancetype)initWithDuration:(NSTimeInterval)duration
                           curve:(UIViewAnimationCurve)curve
                      animations:(void (^__nullable)(void))animations {
  if ((self = [super initWithDuration:duration
                                curve:curve
                           animations:animations])) {
    _hasAnimations = !!animations;
  }
  return self;
}

- (instancetype)initWithDuration:(NSTimeInterval)duration
                   controlPoint1:(CGPoint)point1
                   controlPoint2:(CGPoint)point2
                      animations:(void (^__nullable)(void))animations {
  if ((self = [super initWithDuration:duration
                        controlPoint1:point1
                        controlPoint2:point2
                           animations:animations])) {
    _hasAnimations = !!animations;
  }
  return self;
}

- (instancetype)initWithDuration:(NSTimeInterval)duration
                    dampingRatio:(CGFloat)ratio
                      animations:(void (^__nullable)(void))animations {
  if ((self = [super initWithDuration:duration
                         dampingRatio:ratio
                           animations:animations])) {
    _hasAnimations = !!animations;
  }
  return self;
}

#pragma mark - UIViewImplicitlyAnimating

- (void)addAnimations:(void (^)(void))animation
          delayFactor:(CGFloat)delayFactor {
  if (animation) {
    self.hasAnimations = YES;
  }
  [super addAnimations:animation delayFactor:delayFactor];
}

- (void)addAnimations:(void (^)(void))animation {
  if (animation) {
    self.hasAnimations = YES;
  }
  [super addAnimations:animation];
}

#pragma mark - UIViewAnimating

- (void)startAnimation {
  if (!self.hasAnimations) {
    return;
  }
  [super startAnimation];
}

- (void)startAnimationAfterDelay:(NSTimeInterval)delay {
  if (!self.hasAnimations) {
    return;
  }
  [super startAnimationAfterDelay:delay];
}

@end

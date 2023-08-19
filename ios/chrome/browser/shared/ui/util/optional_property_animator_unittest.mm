// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/optional_property_animator.h"

#import "base/ios/block_types.h"
#import "testing/platform_test.h"

namespace {
// Duration to use for test animators.
const NSTimeInterval kDuration = 0.1;
}  // namespace

// Test fixture for OptionalPropertyAnimator.
using OptionalPropertyAnimatorTest = PlatformTest;

// Tests that the animator's `hasAnimations` property is NO before adding an
// animation.
TEST_F(OptionalPropertyAnimatorTest, NoAnimations) {
  id<UITimingCurveProvider> curve = [[UICubicTimingParameters alloc]
      initWithAnimationCurve:UIViewAnimationCurveEaseOut];
  OptionalPropertyAnimator* animator =
      [[OptionalPropertyAnimator alloc] initWithDuration:kDuration
                                        timingParameters:curve];
  EXPECT_FALSE(animator.hasAnimations);
}

// Tests that animations added in initializers are correctly reflected in
// `hasAnimations`
TEST_F(OptionalPropertyAnimatorTest, InitializersWithAnimations) {
  ProceduralBlock empty_animation = ^{
  };

  OptionalPropertyAnimator* animator = [[OptionalPropertyAnimator alloc]
      initWithDuration:kDuration
                 curve:UIViewAnimationCurveEaseOut
            animations:empty_animation];
  EXPECT_TRUE(animator.hasAnimations);

  animator =
      [[OptionalPropertyAnimator alloc] initWithDuration:kDuration
                                           controlPoint1:CGPointZero
                                           controlPoint2:CGPointZero
                                              animations:empty_animation];
  EXPECT_TRUE(animator.hasAnimations);

  animator =
      [[OptionalPropertyAnimator alloc] initWithDuration:kDuration
                                            dampingRatio:0.0
                                              animations:empty_animation];
  EXPECT_TRUE(animator.hasAnimations);

  animator = [OptionalPropertyAnimator
      runningPropertyAnimatorWithDuration:0.0
                                    delay:0.0
                                  options:0
                               animations:empty_animation
                               completion:nil];
  EXPECT_TRUE(animator.hasAnimations);
}

// Tests that starting an animator with no animations is a no-op.
TEST_F(OptionalPropertyAnimatorTest, NoOpStart) {
  OptionalPropertyAnimator* animator = [[OptionalPropertyAnimator alloc]
      initWithDuration:kDuration
                 curve:UIViewAnimationCurveEaseOut
            animations:nil];
  ASSERT_FALSE(animator.hasAnimations);
  ASSERT_EQ(animator.state, UIViewAnimatingStateInactive);
  ASSERT_FALSE(animator.running);

  // Attempt to start the animator and verify that the state hasn't changed.
  [animator startAnimation];
  EXPECT_EQ(animator.state, UIViewAnimatingStateInactive);
  EXPECT_FALSE(animator.running);

  // Also attempt to start with a delay and ensure that state has still not
  // changed.
  [animator startAnimationAfterDelay:0.0];
  EXPECT_EQ(animator.state, UIViewAnimatingStateInactive);
  EXPECT_FALSE(animator.running);
}

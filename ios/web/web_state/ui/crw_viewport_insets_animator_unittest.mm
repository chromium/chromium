// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_viewport_insets_animator.h"

#import <QuartzCore/QuartzCore.h>

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface CRWViewportInsetsAnimator (Testing)
- (void)handleDisplayLink:(CADisplayLink*)displayLink;
@end

namespace {

using CRWViewportInsetsAnimatorTest = PlatformTest;

// Tests that animating with a positive duration updates insets and fires
// completion upon finish.
TEST_F(CRWViewportInsetsAnimatorTest, AnimateWithDurationRunsToCompletion) {
  const NSTimeInterval duration = 0.4;
  UIEdgeInsets startInsets = UIEdgeInsetsMake(0, 0, 0, 0);
  UIEdgeInsets targetInsets = UIEdgeInsetsMake(100, 0, 80, 0);
  __block BOOL completionCalled = NO;
  __block UIEdgeInsets lastReportedInsets = UIEdgeInsetsZero;

  CRWViewportInsetsAnimator* animator =
      [[CRWViewportInsetsAnimator alloc] initWithStartInsets:startInsets
          targetInsets:targetInsets
          duration:duration
          initialVelocity:0.0
          updateHandler:^(UIEdgeInsets insets) {
            lastReportedInsets = insets;
          }
          completion:^{
            completionCalled = YES;
          }];

  [animator start];

  id mockDisplayLink = OCMClassMock([CADisplayLink class]);
  __block CFTimeInterval mockTimestamp = 100.0;
  [[[mockDisplayLink stub] andDo:^(NSInvocation* invocation) {
    [invocation setReturnValue:&mockTimestamp];
  }] timestamp];

  // Tick 1: start (t = 0.0)
  mockTimestamp = 100.0;
  [animator handleDisplayLink:mockDisplayLink];
  EXPECT_FALSE(completionCalled);

  // Tick 2: end (t = 0.4s)
  mockTimestamp = 100.4;
  [animator handleDisplayLink:mockDisplayLink];

  EXPECT_TRUE(completionCalled);
  EXPECT_TRUE(
      UIEdgeInsetsEqualToEdgeInsets(targetInsets, animator.currentInsets));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(targetInsets, lastReportedInsets));
}

// Tests that stop cancels active animation without invoking completion or
// further updates.
TEST_F(CRWViewportInsetsAnimatorTest, StopAnimationCancelsDisplayLink) {
  __block BOOL completionCalled = NO;
  __block int updateCount = 0;
  CRWViewportInsetsAnimator* animator =
      [[CRWViewportInsetsAnimator alloc] initWithStartInsets:UIEdgeInsetsZero
          targetInsets:UIEdgeInsetsMake(50, 0, 50, 0)
          duration:0.3
          initialVelocity:0.0
          updateHandler:^(UIEdgeInsets insets) {
            updateCount++;
          }
          completion:^{
            completionCalled = YES;
          }];

  [animator start];
  [animator stop];

  EXPECT_FALSE(completionCalled);
  EXPECT_EQ(0, updateCount);
}

// Tests that high initial velocity producing spring overshoot does not result
// in negative insets.
TEST_F(CRWViewportInsetsAnimatorTest,
       SpringOvershootClampsInsetsToNonNegative) {
  const NSTimeInterval duration = 0.2;
  UIEdgeInsets startInsets = UIEdgeInsetsMake(0, 0, 50, 0);
  UIEdgeInsets targetInsets = UIEdgeInsetsZero;
  __block BOOL reportedNegativeInset = NO;
  __block UIEdgeInsets lastReportedInsets = UIEdgeInsetsZero;

  CRWViewportInsetsAnimator* animator = [[CRWViewportInsetsAnimator alloc]
      initWithStartInsets:startInsets
             targetInsets:targetInsets
                 duration:duration
          initialVelocity:200.0
            updateHandler:^(UIEdgeInsets insets) {
              lastReportedInsets = insets;
              if (insets.top < 0 || insets.left < 0 || insets.bottom < 0 ||
                  insets.right < 0) {
                reportedNegativeInset = YES;
              }
            }
               completion:nil];

  [animator start];

  id mockDisplayLink = OCMClassMock([CADisplayLink class]);
  __block CFTimeInterval mockTimestamp = 100.0;
  [[[mockDisplayLink stub] andDo:^(NSInvocation* invocation) {
    [invocation setReturnValue:&mockTimestamp];
  }] timestamp];

  // Tick 1: start (t = 0.0)
  mockTimestamp = 100.0;
  [animator handleDisplayLink:mockDisplayLink];

  // Tick 2: intermediate tick where spring overshoots (t = 0.05s)
  mockTimestamp = 100.05;
  [animator handleDisplayLink:mockDisplayLink];

  EXPECT_FALSE(reportedNegativeInset);
  EXPECT_GE(animator.currentInsets.bottom, 0.0);
  EXPECT_GE(lastReportedInsets.bottom, 0.0);

  // Tick 3: end (t = 0.2s)
  mockTimestamp = 100.2;
  [animator handleDisplayLink:mockDisplayLink];

  EXPECT_FALSE(reportedNegativeInset);
  EXPECT_TRUE(
      UIEdgeInsetsEqualToEdgeInsets(targetInsets, animator.currentInsets));
}

}  // namespace

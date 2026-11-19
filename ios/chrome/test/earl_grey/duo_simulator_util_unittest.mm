// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/duo_simulator_util.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr CGFloat kCoverDisplayShortDimension = 393.0;
constexpr CGFloat kCoverDisplayLongDimension = 852.0;
constexpr CGFloat kInnerDisplayShortDimension = 669.0;
constexpr CGFloat kInnerDisplayLongDimension = 951.0;

using DuoSimulatorUtilTest = PlatformTest;

// Tests that `IsDuoCoverDisplaySize` returns `false` for an empty size.
TEST_F(DuoSimulatorUtilTest, FalseWhenSizeIsEmpty) {
  EXPECT_FALSE(IsDuoCoverDisplaySize(CGSizeZero));
}

// Tests that `IsDuoCoverDisplaySize` returns `true` for the cover display
// dimensions in both portrait and landscape orientations.
TEST_F(DuoSimulatorUtilTest, TrueForCoverDisplay) {
  EXPECT_TRUE(IsDuoCoverDisplaySize(
      CGSizeMake(kCoverDisplayShortDimension, kCoverDisplayLongDimension)));
  EXPECT_TRUE(IsDuoCoverDisplaySize(
      CGSizeMake(kCoverDisplayLongDimension, kCoverDisplayShortDimension)));
}

// Tests that `IsDuoCoverDisplaySize` returns `false` for the inner display
// dimensions in both portrait and landscape orientations.
TEST_F(DuoSimulatorUtilTest, FalseForInnerDisplay) {
  EXPECT_FALSE(IsDuoCoverDisplaySize(
      CGSizeMake(kInnerDisplayLongDimension, kInnerDisplayShortDimension)));
  EXPECT_FALSE(IsDuoCoverDisplaySize(
      CGSizeMake(kInnerDisplayShortDimension, kInnerDisplayLongDimension)));
}

#if TARGET_OS_SIMULATOR
// Tests that `PrepareForSimulatedDuoHingeAngle` skips transitions when the key
// window is already settled on the cover display, and requests transitions
// otherwise.
TEST_F(DuoSimulatorUtilTest, PrepareForSimulatedDuoHingeAngleOnCoverDisplay) {
  ScopedKeyWindow scoped_key_window;
  UIWindow* window = scoped_key_window.Get();
  window.frame =
      CGRectMake(0, 0, kCoverDisplayShortDimension, kCoverDisplayLongDimension);

  EXPECT_FALSE(PrepareForSimulatedDuoHingeAngle(0.0));
  EXPECT_TRUE(IsSimulatedDuoHingePostureSettled(0.0));
  EXPECT_TRUE(PrepareForSimulatedDuoHingeAngle(130.0));
  EXPECT_TRUE(PrepareForSimulatedDuoHingeAngle(180.0));
}

// Tests that `PrepareForSimulatedDuoHingeAngle` requests a transition to closed
// when the key window is sized for the inner display.
TEST_F(DuoSimulatorUtilTest, PrepareForSimulatedDuoHingeAngleOnInnerDisplay) {
  ScopedKeyWindow scoped_key_window;
  UIWindow* window = scoped_key_window.Get();
  window.frame =
      CGRectMake(0, 0, kInnerDisplayLongDimension, kInnerDisplayShortDimension);

  EXPECT_TRUE(PrepareForSimulatedDuoHingeAngle(0.0));
  EXPECT_FALSE(IsSimulatedDuoHingePostureSettled(0.0));
}

// Tests that an active layer animation causes
// `PrepareForSimulatedDuoHingeAngle` to wait until animations finish, and
// settling succeeds once animations end.
TEST_F(DuoSimulatorUtilTest, PrepareForSimulatedDuoHingeAngleDuringAnimation) {
  ScopedKeyWindow scoped_key_window;
  UIWindow* window = scoped_key_window.Get();
  window.frame =
      CGRectMake(0, 0, kCoverDisplayShortDimension, kCoverDisplayLongDimension);

  CABasicAnimation* animation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  animation.duration = 10.0;
  [window.layer addAnimation:animation forKey:@"test_animation"];

  EXPECT_FALSE(IsSimulatedDuoHingePostureSettled(0.0));
  EXPECT_TRUE(PrepareForSimulatedDuoHingeAngle(0.0));

  [window.layer removeAllAnimations];
  EXPECT_TRUE(IsSimulatedDuoHingePostureSettled(0.0));
  EXPECT_FALSE(PrepareForSimulatedDuoHingeAngle(0.0));
}
#endif  // TARGET_OS_SIMULATOR

}  // namespace

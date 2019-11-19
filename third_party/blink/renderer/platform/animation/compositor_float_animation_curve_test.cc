// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"

#include <memory>

#include "cc/animation/timing_function.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::CompositorAnimationCurve;
using blink::CompositorFloatAnimationCurve;
using blink::CompositorFloatKeyframe;

namespace blink {

// Tests that a float animation with one keyframe works as expected.
TEST(WebFloatAnimationCurveTest, OneFloatKeyframe) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::Shared()));
  EXPECT_FLOAT_EQ(2, curve->GetValue(-1));
  EXPECT_FLOAT_EQ(2, curve->GetValue(0));
  EXPECT_FLOAT_EQ(2, curve->GetValue(0.5));
  EXPECT_FLOAT_EQ(2, curve->GetValue(1));
  EXPECT_FLOAT_EQ(2, curve->GetValue(2));
}

// Tests that a float animation with two keyframes works as expected.
TEST(WebFloatAnimationCurveTest, TwoFloatKeyframe) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::Shared()));
  EXPECT_FLOAT_EQ(2, curve->GetValue(-1));
  EXPECT_FLOAT_EQ(2, curve->GetValue(0));
  EXPECT_FLOAT_EQ(3, curve->GetValue(0.5));
  EXPECT_FLOAT_EQ(4, curve->GetValue(1));
  EXPECT_FLOAT_EQ(4, curve->GetValue(2));
}

// Tests that a float animation with three keyframes works as expected.
TEST(WebFloatAnimationCurveTest, ThreeFloatKeyframe) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(2, 8, *LinearTimingFunction::Shared()));
  EXPECT_FLOAT_EQ(2, curve->GetValue(-1));
  EXPECT_FLOAT_EQ(2, curve->GetValue(0));
  EXPECT_FLOAT_EQ(3, curve->GetValue(0.5));
  EXPECT_FLOAT_EQ(4, curve->GetValue(1));
  EXPECT_FLOAT_EQ(6, curve->GetValue(1.5));
  EXPECT_FLOAT_EQ(8, curve->GetValue(2));
  EXPECT_FLOAT_EQ(8, curve->GetValue(3));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(WebFloatAnimationCurveTest, RepeatedFloatKeyTimes) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 4, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 6, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(2, 6, *LinearTimingFunction::Shared()));

  EXPECT_FLOAT_EQ(4, curve->GetValue(-1));
  EXPECT_FLOAT_EQ(4, curve->GetValue(0));
  EXPECT_FLOAT_EQ(4, curve->GetValue(0.5));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  float value = curve->GetValue(1);
  EXPECT_TRUE(value >= 4 && value <= 6);

  EXPECT_FLOAT_EQ(6, curve->GetValue(1.5));
  EXPECT_FLOAT_EQ(6, curve->GetValue(2));
  EXPECT_FLOAT_EQ(6, curve->GetValue(3));
}

// Tests that the keyframes may be added out of order.
TEST(WebFloatAnimationCurveTest, UnsortedKeyframes) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(2, 8, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::Shared()));

  EXPECT_FLOAT_EQ(2, curve->GetValue(-1));
  EXPECT_FLOAT_EQ(2, curve->GetValue(0));
  EXPECT_FLOAT_EQ(3, curve->GetValue(0.5));
  EXPECT_FLOAT_EQ(4, curve->GetValue(1));
  EXPECT_FLOAT_EQ(6, curve->GetValue(1.5));
  EXPECT_FLOAT_EQ(8, curve->GetValue(2));
  EXPECT_FLOAT_EQ(8, curve->GetValue(3));
}

// Tests that a cubic bezier timing function works as expected.
TEST(WebFloatAnimationCurveTest, CubicBezierTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  scoped_refptr<CubicBezierTimingFunction> cubic =
      CubicBezierTimingFunction::Create(0.25, 0, 0.75, 1);
  curve->AddKeyframe(CompositorFloatKeyframe(0, 0, *cubic));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  EXPECT_FLOAT_EQ(0, curve->GetValue(0));
  EXPECT_LT(0, curve->GetValue(0.25));
  EXPECT_GT(0.25, curve->GetValue(0.25));
  EXPECT_NEAR(curve->GetValue(0.5), 0.5, 0.00015);
  EXPECT_LT(0.75, curve->GetValue(0.75));
  EXPECT_GT(1, curve->GetValue(0.75));
  EXPECT_FLOAT_EQ(1, curve->GetValue(1));
}

// Tests that an ease timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 0,
                              *CubicBezierTimingFunction::Preset(
                                  CubicBezierTimingFunction::EaseType::EASE)));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_NEAR(timing_function->GetValue(time), curve->GetValue(time), 1e-7);
  }
}

// Tests using a linear timing function.
TEST(WebFloatAnimationCurveTest, LinearTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 0, *LinearTimingFunction::Shared()));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(time, curve->GetValue(time));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(CompositorFloatKeyframe(
      0, 0,
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN)));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_NEAR(timing_function->GetValue(time), curve->GetValue(time), 1e-7);
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseOutTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(CompositorFloatKeyframe(
      0, 0,
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT)));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_OUT));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_NEAR(timing_function->GetValue(time), curve->GetValue(time), 1e-7);
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInOutTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(CompositorFloatKeyframe(
      0, 0,
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_NEAR(timing_function->GetValue(time), curve->GetValue(time), 1e-7);
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, CustomBezierTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  double x1 = 0.3;
  double y1 = 0.2;
  double x2 = 0.8;
  double y2 = 0.7;
  scoped_refptr<CubicBezierTimingFunction> cubic =
      CubicBezierTimingFunction::Create(x1, y1, x2, y2);
  curve->AddKeyframe(CompositorFloatKeyframe(0, 0, *cubic));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_NEAR(timing_function->GetValue(time), curve->GetValue(time), 1e-7);
  }
}

// Tests that the default timing function is indeed ease.
TEST(WebFloatAnimationCurveTest, DefaultTimingFunction) {
  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0, 0,
                              *CubicBezierTimingFunction::Preset(
                                  CubicBezierTimingFunction::EaseType::EASE)));
  curve->AddKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_NEAR(timing_function->GetValue(time), curve->GetValue(time), 1e-7);
  }
}

}  // namespace blink

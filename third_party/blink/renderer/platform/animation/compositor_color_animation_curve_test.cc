// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_color_animation_curve.h"

#include <memory>

#include "cc/animation/timing_function.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::CompositorAnimationCurve;
using blink::CompositorColorAnimationCurve;
using blink::CompositorColorKeyframe;

namespace blink {

// Tests that a color animation with one keyframe works as expected.
TEST(WebColorAnimationCurveTest, OneColorKeyframe) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(0, SK_ColorGREEN,
                                             *LinearTimingFunction::Shared()));
  EXPECT_EQ(SK_ColorGREEN, curve->GetValue(-1));
  EXPECT_EQ(SK_ColorGREEN, curve->GetValue(0));
  EXPECT_EQ(SK_ColorGREEN, curve->GetValue(0.5));
  EXPECT_EQ(SK_ColorGREEN, curve->GetValue(1));
  EXPECT_EQ(SK_ColorGREEN, curve->GetValue(2));
}

// Tests that a color animation with two keyframes works as expected.
TEST(WebColorAnimationCurveTest, TwoColorKeyframe) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(0, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 200, 100),
                                             *LinearTimingFunction::Shared()));
  EXPECT_EQ(SkColorSetRGB(0, 100, 0), curve->GetValue(-1));
  EXPECT_EQ(SkColorSetRGB(0, 100, 0), curve->GetValue(0));
  EXPECT_EQ(SkColorSetRGB(0, 150, 50), curve->GetValue(0.5));
  EXPECT_EQ(SkColorSetRGB(0, 200, 100), curve->GetValue(1));
  EXPECT_EQ(SkColorSetRGB(0, 200, 100), curve->GetValue(2));
}

// Tests that a color animation with changing alpha channel
TEST(WebColorAnimationCurveTest, TwoAlphaKeyframe) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(0, SkColorSetARGB(100, 0, 100, 0),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(
      1, SkColorSetARGB(255, 0, 200, 100), *LinearTimingFunction::Shared()));
  EXPECT_EQ(SkColorSetARGB(100, 0, 100, 0), curve->GetValue(-1));
  EXPECT_EQ(SkColorSetARGB(100, 0, 100, 0), curve->GetValue(0));
  EXPECT_EQ(SkColorSetARGB(178, 0, 172, 72), curve->GetValue(0.5));
  EXPECT_EQ(SkColorSetARGB(255, 0, 200, 100), curve->GetValue(1));
  EXPECT_EQ(SkColorSetARGB(255, 0, 200, 100), curve->GetValue(2));
}

// Tests that a color animation with three keyframes works as expected.
TEST(WebColorAnimationCurveTest, ThreeColorKeyframe) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(0, SkColorSetRGB(0, 50, 0),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 50),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(2, SkColorSetRGB(0, 200, 200),
                                             *LinearTimingFunction::Shared()));
  EXPECT_EQ(SkColorSetRGB(0, 50, 0), curve->GetValue(-1));
  EXPECT_EQ(SkColorSetRGB(0, 50, 0), curve->GetValue(0));
  EXPECT_EQ(SkColorSetRGB(0, 75, 25), curve->GetValue(0.5));
  EXPECT_EQ(SkColorSetRGB(0, 100, 50), curve->GetValue(1));
  EXPECT_EQ(SkColorSetRGB(0, 150, 125), curve->GetValue(1.5));
  EXPECT_EQ(SkColorSetRGB(0, 200, 200), curve->GetValue(2));
  EXPECT_EQ(SkColorSetRGB(0, 200, 200), curve->GetValue(3));
}

// Tests that a color animation with multiple keys at a given time works sanely.
TEST(WebColorAnimationCurveTest, RepeatedColorKeyTimes) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(0, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 200, 0),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(2, SkColorSetRGB(0, 200, 0),
                                             *LinearTimingFunction::Shared()));

  EXPECT_EQ(SkColorSetRGB(0, 100, 0), curve->GetValue(-1));
  EXPECT_EQ(SkColorSetRGB(0, 100, 0), curve->GetValue(0));
  EXPECT_EQ(SkColorSetRGB(0, 100, 0), curve->GetValue(0.5));

  // There is a discontinuity at 1. Any value between 100 and 200 in the green
  // channel is valid
  SkColor value = curve->GetValue(1);
  EXPECT_EQ(SkColorGetR(value), 0U);
  EXPECT_TRUE(SkColorGetG(value) >= 100U && SkColorGetG(value) <= 200U);
  EXPECT_EQ(SkColorGetB(value), 0U);

  EXPECT_EQ(SkColorSetRGB(0, 200, 0), curve->GetValue(1.5));
  EXPECT_EQ(SkColorSetRGB(0, 200, 0), curve->GetValue(2));
  EXPECT_EQ(SkColorSetRGB(0, 200, 0), curve->GetValue(3));
}

// Tests that the keyframes may be added out of order.
TEST(WebColorAnimationCurveTest, UnsortedKeyframes) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(2, SkColorSetRGB(0, 200, 200),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(0, SkColorSetRGB(0, 50, 0),
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 50),
                                             *LinearTimingFunction::Shared()));
  EXPECT_EQ(SkColorSetRGB(0, 50, 0), curve->GetValue(-1));
  EXPECT_EQ(SkColorSetRGB(0, 50, 0), curve->GetValue(0));
  EXPECT_EQ(SkColorSetRGB(0, 75, 25), curve->GetValue(0.5));
  EXPECT_EQ(SkColorSetRGB(0, 100, 50), curve->GetValue(1));
  EXPECT_EQ(SkColorSetRGB(0, 150, 125), curve->GetValue(1.5));
  EXPECT_EQ(SkColorSetRGB(0, 200, 200), curve->GetValue(2));
  EXPECT_EQ(SkColorSetRGB(0, 200, 200), curve->GetValue(3));
}

// Tests that a cubic bezier timing function works as expected.
TEST(WebColorAnimationCurveTest, CubicBezierTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  scoped_refptr<CubicBezierTimingFunction> cubic =
      CubicBezierTimingFunction::Create(0.25, 0, 0.75, 1);
  curve->AddKeyframe(CompositorColorKeyframe(0, SK_ColorBLACK, *cubic));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  EXPECT_EQ(0U, SkColorGetG(curve->GetValue(0)));
  EXPECT_LT(0U, SkColorGetG(curve->GetValue(0.25)));
  EXPECT_GT(25U, SkColorGetG(curve->GetValue(0.25)));
  EXPECT_EQ(50U, SkColorGetG(curve->GetValue(0.5)));
  EXPECT_LT(75U, SkColorGetG(curve->GetValue(0.75)));
  EXPECT_GT(100U, SkColorGetG(curve->GetValue(0.75)));
  EXPECT_EQ(100U, SkColorGetG(curve->GetValue(1)));
}

// Tests that an ease timing function works as expected.
TEST(WebColorAnimationCurveTest, EaseTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(
      CompositorColorKeyframe(0, SK_ColorBLACK,
                              *CubicBezierTimingFunction::Preset(
                                  CubicBezierTimingFunction::EaseType::EASE)));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_EQ((unsigned)round(timing_function->GetValue(time) * 100),
              SkColorGetG(curve->GetValue(time)));
  }
}

// Tests using a linear timing function.
TEST(WebColorAnimationCurveTest, LinearTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(0, SK_ColorBLACK,
                                             *LinearTimingFunction::Shared()));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  for (int i = 0; i <= 4; ++i) {
    EXPECT_EQ(i * 25U, SkColorGetG(curve->GetValue(i * 0.25)));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebColorAnimationCurveTest, EaseInTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(
      0, SK_ColorBLACK,
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN)));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_EQ((unsigned)round(timing_function->GetValue(time) * 100),
              SkColorGetG(curve->GetValue(time)));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebColorAnimationCurveTest, EaseOutTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(
      0, SK_ColorBLACK,
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_OUT)));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_OUT));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_EQ((unsigned)round(timing_function->GetValue(time) * 100),
              SkColorGetG(curve->GetValue(time)));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebColorAnimationCurveTest, EaseInOutTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(CompositorColorKeyframe(
      0, SK_ColorBLACK,
      *CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_EQ((unsigned)round(timing_function->GetValue(time) * 100),
              SkColorGetG(curve->GetValue(time)));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebColorAnimationCurveTest, CustomBezierTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  double x1 = 0.3;
  double y1 = 0.2;
  double x2 = 0.8;
  double y2 = 0.7;
  scoped_refptr<CubicBezierTimingFunction> cubic =
      CubicBezierTimingFunction::Create(x1, y1, x2, y2);
  curve->AddKeyframe(CompositorColorKeyframe(0, SK_ColorBLACK, *cubic));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_EQ((unsigned)round(timing_function->GetValue(time) * 100),
              SkColorGetG(curve->GetValue(time)));
  }
}

// Tests that the default timing function is indeed ease.
TEST(WebColorAnimationCurveTest, DefaultTimingFunction) {
  auto curve = std::make_unique<CompositorColorAnimationCurve>();
  curve->AddKeyframe(
      CompositorColorKeyframe(0, SK_ColorBLACK,
                              *CubicBezierTimingFunction::Preset(
                                  CubicBezierTimingFunction::EaseType::EASE)));
  curve->AddKeyframe(CompositorColorKeyframe(1, SkColorSetRGB(0, 100, 0),
                                             *LinearTimingFunction::Shared()));

  std::unique_ptr<cc::TimingFunction> timing_function(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_EQ((unsigned)round(timing_function->GetValue(time) * 100),
              SkColorGetG(curve->GetValue(time)));
  }
}

}  // namespace blink

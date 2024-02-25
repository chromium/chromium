// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(CSSColorInterpolationTypeTest, GetRGBA1) {
  test::TaskEnvironment task_environment;
  Color color(230, 120, 0, 255);
  EXPECT_EQ(color,
            CSSColorInterpolationType::GetColor(
                *CSSColorInterpolationType::CreateInterpolableColor(color)));
}

TEST(CSSColorInterpolationTypeTest, GetRGBA2) {
  test::TaskEnvironment task_environment;
  Color color(100, 190, 0, 1);
  EXPECT_EQ(color,
            CSSColorInterpolationType::GetColor(
                *CSSColorInterpolationType::CreateInterpolableColor(color)));
}

TEST(CSSColorInterpolationTypeTest, GetRGBA3) {
  test::TaskEnvironment task_environment;
  Color color(35, 140, 10, 10);
  EXPECT_EQ(color,
            CSSColorInterpolationType::GetColor(
                *CSSColorInterpolationType::CreateInterpolableColor(color)));
}

TEST(CSSColorInterpolationTypeTest, GetRGBA4) {
  test::TaskEnvironment task_environment;
  Color color(35, 140, 10, 0);
  EXPECT_EQ(Color::FromRGBA(0, 0, 0, 0),
            CSSColorInterpolationType::GetColor(
                *CSSColorInterpolationType::CreateInterpolableColor(color)));
}

TEST(CSSColorInterpolationTypeTest, RGBBounds) {
  test::TaskEnvironment task_environment;
  Color from_color(0, 0, 0, 0);
  Color to_color(255, 255, 255, 255);
  InterpolableValue* from =
      CSSColorInterpolationType::CreateInterpolableColor(from_color);
  InterpolableValue* to =
      CSSColorInterpolationType::CreateInterpolableColor(to_color);
  InterpolableValue* result =
      CSSColorInterpolationType::CreateInterpolableColor(to_color);

  from->Interpolate(*to, 1e30, *result);
  Color rgba = CSSColorInterpolationType::GetColor(*result);
  ASSERT_EQ(255, rgba.Red());
  ASSERT_EQ(255, rgba.Green());
  ASSERT_EQ(255, rgba.Blue());
  ASSERT_EQ(255, rgba.AlphaAsInteger());
}

TEST(CSSColorInterpolationTypeTest, RGBToOklab) {
  test::TaskEnvironment task_environment;
  Color from_color = Color::FromRGBAFloat(1, 1, 1, 1);
  Color to_color =
      Color::FromColorSpace(Color::ColorSpace::kOklab, 0, 0, 0, 0.5);
  InterpolableColor* from =
      CSSColorInterpolationType::CreateInterpolableColor(from_color);
  InterpolableColor* to =
      CSSColorInterpolationType::CreateInterpolableColor(to_color);

  from_color = CSSColorInterpolationType::GetColor(*from);
  ASSERT_EQ(Color::ColorSpace::kSRGBLegacy,
            from_color.GetColorInterpolationSpace());
  to_color = CSSColorInterpolationType::GetColor(*to);
  ASSERT_EQ(Color::ColorSpace::kOklab, to_color.GetColorInterpolationSpace());

  // This should make both color interpolations spaces oklab
  InterpolableColor::SetupColorInterpolationSpaces(*to, *from);

  from_color = CSSColorInterpolationType::GetColor(*from);
  ASSERT_EQ(Color::ColorSpace::kOklab, from_color.GetColorInterpolationSpace());
  to_color = CSSColorInterpolationType::GetColor(*to);
  ASSERT_EQ(Color::ColorSpace::kOklab, to_color.GetColorInterpolationSpace());
}

TEST(CSSColorInterpolationTypeTest, Oklab) {
  test::TaskEnvironment task_environment;
  Color from_color =
      Color::FromColorSpace(Color::ColorSpace::kOklab, 1, 1, 1, 1);
  Color to_color =
      Color::FromColorSpace(Color::ColorSpace::kOklab, 0, 0, 0, 0.5);
  InterpolableValue* from =
      CSSColorInterpolationType::CreateInterpolableColor(from_color);
  InterpolableValue* to =
      CSSColorInterpolationType::CreateInterpolableColor(to_color);
  InterpolableValue* result =
      CSSColorInterpolationType::CreateInterpolableColor(to_color);

  Color result_color;
  from->Interpolate(*to, 0, *result);
  result_color = CSSColorInterpolationType::GetColor(*result);
  ASSERT_EQ(1, result_color.Param0());
  ASSERT_EQ(1, result_color.Param1());
  ASSERT_EQ(1, result_color.Param2());
  ASSERT_EQ(1, result_color.Alpha());
  ASSERT_EQ(Color::ColorSpace::kOklab,
            result_color.GetColorInterpolationSpace());

  from->Interpolate(*to, 0.5, *result);
  result_color = CSSColorInterpolationType::GetColor(*result);
  // Everything is premultiplied.
  ASSERT_EQ(0.5, result_color.Param0() * result_color.Alpha());
  ASSERT_EQ(0.5, result_color.Param1() * result_color.Alpha());
  ASSERT_EQ(0.5, result_color.Param2() * result_color.Alpha());
  ASSERT_EQ(0.75, result_color.Alpha());
  ASSERT_EQ(Color::ColorSpace::kOklab,
            result_color.GetColorInterpolationSpace());

  from->Interpolate(*to, 0.75, *result);
  result_color = CSSColorInterpolationType::GetColor(*result);
  // Everything is premultiplied.
  ASSERT_EQ(0.25, result_color.Param0() * result_color.Alpha());
  ASSERT_EQ(0.25, result_color.Param1() * result_color.Alpha());
  ASSERT_EQ(0.25, result_color.Param2() * result_color.Alpha());
  ASSERT_EQ(0.625, result_color.Alpha());
  ASSERT_EQ(Color::ColorSpace::kOklab,
            result_color.GetColorInterpolationSpace());

  from->Interpolate(*to, 1, *result);
  result_color = CSSColorInterpolationType::GetColor(*result);
  ASSERT_EQ(0, result_color.Param0());
  ASSERT_EQ(0, result_color.Param1());
  ASSERT_EQ(0, result_color.Param2());
  ASSERT_EQ(0.5, result_color.Alpha());
  ASSERT_EQ(Color::ColorSpace::kOklab,
            result_color.GetColorInterpolationSpace());
}

}  // namespace blink

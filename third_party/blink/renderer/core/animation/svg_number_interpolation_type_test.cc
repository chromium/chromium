// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_number_interpolation_type.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/svg/svg_number.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(SVGNumberInterpolationTypeTest, NonNegativeSVGNumber) {
  test::TaskEnvironment task_environment;
  // kPathLengthAttr implies non-negative.
  SVGNumberInterpolationType interpolation_type(svg_names::kPathLengthAttr);

  SVGNumber* svg_number =
      static_cast<SVGNumber*>(interpolation_type.AppliedSVGValueForTesting(
          InterpolableNumber(5), nullptr));
  EXPECT_EQ(svg_number->Value(), 5);

  svg_number =
      static_cast<SVGNumber*>(interpolation_type.AppliedSVGValueForTesting(
          InterpolableNumber(-5), nullptr));
  EXPECT_EQ(svg_number->Value(), 0);
}

TEST(SVGNumberInterpolationTypeTest, NegativeSVGNumber) {
  test::TaskEnvironment task_environment;
  // kOffsetAttr can be negative.
  SVGNumberInterpolationType interpolation_type(svg_names::kOffsetAttr);

  SVGNumber* svg_number =
      static_cast<SVGNumber*>(interpolation_type.AppliedSVGValueForTesting(
          InterpolableNumber(5), nullptr));
  EXPECT_EQ(svg_number->Value(), 5);

  svg_number =
      static_cast<SVGNumber*>(interpolation_type.AppliedSVGValueForTesting(
          InterpolableNumber(-5), nullptr));
  EXPECT_EQ(svg_number->Value(), -5);
}

// This is a regression test for https://crbug.com/961859. InterpolableNumber
// can represent a double, but SVGNumber is created from a float, so we must
// make sure to clamp it.
TEST(SVGNumberInterpolationTypeTest, InterpolableNumberOutOfRange) {
  test::TaskEnvironment task_environment;
  SVGNumberInterpolationType interpolation_type(svg_names::kOffsetAttr);

  double too_large = std::numeric_limits<float>::max() * 2;
  SVGNumber* svg_number =
      static_cast<SVGNumber*>(interpolation_type.AppliedSVGValueForTesting(
          InterpolableNumber(too_large), nullptr));
  EXPECT_EQ(svg_number->Value(), std::numeric_limits<float>::max());
}

}  // namespace blink

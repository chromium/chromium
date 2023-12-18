// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(PhysicalSizeTest, MultipleFloat) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(PhysicalSize(100, 7), PhysicalSize(200, 14) * 0.5f);
  EXPECT_EQ(PhysicalSize(-100, -7), PhysicalSize(200, 14) * -0.5f);
  EXPECT_EQ(PhysicalSize(0, 0),
            PhysicalSize(200, 14) * std::numeric_limits<float>::quiet_NaN());
}

TEST(PhysicalSizeTest, ExpandedTo) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(PhysicalSize(13, 7), PhysicalSize(13, 1).ExpandedTo({10, 7}));
  EXPECT_EQ(PhysicalSize(17, 1), PhysicalSize(13, 1).ExpandedTo({17, 1}));
}

TEST(PhysicalSizeTest, ShrunkTo) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(PhysicalSize(10, 1), PhysicalSize(13, 1).ShrunkTo({10, 7}));
  EXPECT_EQ(PhysicalSize(13, -1), PhysicalSize(13, 1).ShrunkTo({14, -1}));
}

TEST(PhysicalSizeTest, FitToAspectRatioShrink) {
  test::TaskEnvironment task_environment;
  PhysicalSize aspect_ratio(50000, 40000);
  EXPECT_EQ(PhysicalSize(1250, 1000),
            PhysicalSize(2000, 1000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitShrink));
  EXPECT_EQ(PhysicalSize(1000, 800),
            PhysicalSize(1000, 2000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitShrink));

  PhysicalSize aspect_ratio2(1140, 696);
  PhysicalSize ref_size(
      LayoutUnit(350),
      LayoutUnit(350).MulDiv(aspect_ratio2.height, aspect_ratio2.width));
  EXPECT_EQ(ref_size,
            ref_size.FitToAspectRatio(aspect_ratio2, kAspectRatioFitShrink));
}

TEST(PhysicalSizeTest, FitToAspectRatioGrow) {
  test::TaskEnvironment task_environment;
  PhysicalSize aspect_ratio(50000, 40000);
  EXPECT_EQ(PhysicalSize(2000, 1600),
            PhysicalSize(2000, 1000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitGrow));
  EXPECT_EQ(PhysicalSize(2500, 2000),
            PhysicalSize(1000, 2000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitGrow));

  PhysicalSize aspect_ratio2(1140, 696);
  PhysicalSize ref_size(
      LayoutUnit(350),
      LayoutUnit(350).MulDiv(aspect_ratio2.height, aspect_ratio2.width));
  EXPECT_EQ(ref_size,
            ref_size.FitToAspectRatio(aspect_ratio2, kAspectRatioFitGrow));
}

}  // namespace blink

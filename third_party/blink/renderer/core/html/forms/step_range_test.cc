// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/step_range.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(StepRangeTest, ClampValueWithOutStepMatchedValue) {
  test::TaskEnvironment task_environment;
  // <input type=range value=200 min=0 max=100 step=1000>
  StepRange step_range(Decimal(200), Decimal(0), Decimal(100), true,
                       /*supports_reversed_range=*/false, Decimal(1000),
                       StepRange::StepDescription());

  EXPECT_EQ(Decimal(100), step_range.ClampValue(Decimal(200)));
  EXPECT_EQ(Decimal(0), step_range.ClampValue(Decimal(-100)));
}

TEST(StepRangeTest, StepSnappedMaximum) {
  test::TaskEnvironment task_environment;
  // <input type=number value="1110" max=100 step="20">
  StepRange step_range(Decimal::FromDouble(1110), Decimal(0), Decimal(100),
                       true, /*supports_reversed_range=*/false, Decimal(20),
                       StepRange::StepDescription());
  EXPECT_EQ(Decimal(90), step_range.StepSnappedMaximum());

  // crbug.com/617809
  // <input type=number
  // value="8624024784918570374158793713225864658725102756338798521486349461900449498315865014065406918592181034633618363349807887404915072776534917803019477033072906290735591367789665757384135591225430117374220731087966"
  // min=0 max=100 step="18446744073709551575">
  StepRange step_range2(Decimal::FromDouble(8.62402e+207), Decimal(0),
                        Decimal(100), true, /*supports_reversed_range=*/false,
                        Decimal::FromDouble(1.84467e+19),
                        StepRange::StepDescription());
  EXPECT_FALSE(step_range2.StepSnappedMaximum().IsFinite());

  StepRange step_range3(Decimal::FromDouble(100), Decimal(0), Decimal(400),
                        true, /*supports_reversed_range=*/false, Decimal(-7),
                        StepRange::StepDescription());
  EXPECT_FALSE(step_range3.StepSnappedMaximum().IsFinite());
}

TEST(StepRangeTest, ReversedRange) {
  test::TaskEnvironment task_environment;
  // <input type=time min="23:00" max="01:00">
  StepRange reversed_time_range(
      /*step_base=*/Decimal::FromDouble(82800000),
      /*minimum=*/Decimal::FromDouble(82800000),
      /*maximum=*/Decimal::FromDouble(3600000),
      /*has_range_limitations=*/true,
      /*supports_reversed_range=*/true,
      /*step=*/Decimal::FromDouble(60000),
      /*step_description=*/StepRange::StepDescription());
  EXPECT_TRUE(reversed_time_range.HasReversedRange());

  // <input type=time min="01:00" max="23:00">
  StepRange regular_time_range(
      /*step_base=*/Decimal::FromDouble(3600000),
      /*minimum=*/Decimal::FromDouble(3600000),
      /*maximum=*/Decimal::FromDouble(82800000),
      /*has_range_limitations=*/true,
      /*supports_reversed_range=*/true,
      /*step=*/Decimal::FromDouble(60000),
      /*step_description=*/StepRange::StepDescription());
  EXPECT_FALSE(regular_time_range.HasReversedRange());
}

}  // namespace blink

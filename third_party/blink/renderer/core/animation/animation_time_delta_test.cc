// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_time_delta.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(AnimationTimeDeltaTest, Construction) {
  test::TaskEnvironment task_environment;
  // The default constructor is a zero-length delta.
  EXPECT_EQ(AnimationTimeDelta(), ANIMATION_TIME_DELTA_FROM_SECONDS(0));
  EXPECT_EQ(AnimationTimeDelta(), ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(5.5),
            ANIMATION_TIME_DELTA_FROM_MILLISECONDS(5500));
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(-2),
            ANIMATION_TIME_DELTA_FROM_MILLISECONDS(-2000));
}

TEST(AnimationTimeDeltaTest, Conversion) {
  test::TaskEnvironment task_environment;
  AnimationTimeDelta delta = ANIMATION_TIME_DELTA_FROM_SECONDS(5);
  EXPECT_EQ(delta.InSecondsF(), 5);
  EXPECT_EQ(delta.InMillisecondsF(), 5000);

  delta = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(1234);
  EXPECT_EQ(delta.InSecondsF(), 1.234);
  EXPECT_EQ(delta.InMillisecondsF(), 1234);
}

TEST(AnimationTimeDeltaTest, Max) {
  test::TaskEnvironment task_environment;
  AnimationTimeDelta max_delta = AnimationTimeDelta::Max();
  EXPECT_TRUE(max_delta.is_max());
  EXPECT_EQ(max_delta, AnimationTimeDelta::Max());
  EXPECT_GT(max_delta, ANIMATION_TIME_DELTA_FROM_SECONDS(365 * 24 * 60 * 60));

  EXPECT_EQ(max_delta.InSecondsF(), std::numeric_limits<double>::infinity());
  EXPECT_EQ(max_delta.InMillisecondsF(),
            std::numeric_limits<double>::infinity());
}

TEST(AnimationTimeDeltaTest, Zero) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(AnimationTimeDelta().is_zero());
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(0).is_zero());
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0).is_zero());

  EXPECT_FALSE(ANIMATION_TIME_DELTA_FROM_SECONDS(54.5).is_zero());
  EXPECT_FALSE(ANIMATION_TIME_DELTA_FROM_SECONDS(-0.5).is_zero());
  EXPECT_FALSE(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(123.45).is_zero());
}

TEST(AnimationTimeDeltaTest, Computation) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(4.5) +
                ANIMATION_TIME_DELTA_FROM_MILLISECONDS(500),
            ANIMATION_TIME_DELTA_FROM_SECONDS(5));
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(100) +
                ANIMATION_TIME_DELTA_FROM_MILLISECONDS(-850),
            ANIMATION_TIME_DELTA_FROM_SECONDS(99.15));

  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(5) * 20,
            ANIMATION_TIME_DELTA_FROM_SECONDS(100));
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(10) * 1.5,
            ANIMATION_TIME_DELTA_FROM_SECONDS(15));
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(2.5) * -2,
            ANIMATION_TIME_DELTA_FROM_SECONDS(-5));

  EXPECT_EQ(20 * ANIMATION_TIME_DELTA_FROM_SECONDS(5),
            ANIMATION_TIME_DELTA_FROM_SECONDS(100));
}

TEST(AnimationTimeDeltaTest, Comparison) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(10) ==
              ANIMATION_TIME_DELTA_FROM_SECONDS(10));
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(10) !=
              ANIMATION_TIME_DELTA_FROM_SECONDS(50));
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(50) >
              ANIMATION_TIME_DELTA_FROM_SECONDS(49.999));
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(50) >=
              ANIMATION_TIME_DELTA_FROM_SECONDS(49.999));
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(50) >=
              ANIMATION_TIME_DELTA_FROM_SECONDS(50));
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(50) <=
              ANIMATION_TIME_DELTA_FROM_SECONDS(50));
  EXPECT_TRUE(ANIMATION_TIME_DELTA_FROM_SECONDS(50) <=
              ANIMATION_TIME_DELTA_FROM_SECONDS(100));
}

TEST(AnimationTimeDeltaTest, Division) {
  test::TaskEnvironment task_environment;
  double inf = std::numeric_limits<double>::infinity();
  AnimationTimeDelta inf_time_delta = AnimationTimeDelta::Max();
  AnimationTimeDelta zero = AnimationTimeDelta();
  AnimationTimeDelta num = ANIMATION_TIME_DELTA_FROM_SECONDS(5);

  // 0 / 0 = undefined
  EXPECT_DEATH_IF_SUPPORTED(zero / zero, "");
  // 0 / inf = 0
  EXPECT_EQ(0, zero / inf_time_delta);
  // 0 / -inf = 0
  EXPECT_EQ(0, zero / -inf_time_delta);
  // 0 / 5 = 0
  EXPECT_EQ(0, zero / num);
  // inf / 0 = undefined
  EXPECT_DEATH_IF_SUPPORTED(inf_time_delta / zero, "");
  // -inf / 0 = undefined
  EXPECT_DEATH_IF_SUPPORTED(-inf_time_delta / zero, "");
  // inf / inf = undefined
  EXPECT_DEATH_IF_SUPPORTED(inf_time_delta / inf_time_delta, "");
  // inf / -inf = undefined
  EXPECT_DEATH_IF_SUPPORTED(inf_time_delta / -inf_time_delta, "");
  // -inf / inf = undefined
  EXPECT_DEATH_IF_SUPPORTED(-inf_time_delta / inf_time_delta, "");
  // -inf / -inf = undefined
  EXPECT_DEATH_IF_SUPPORTED(-inf_time_delta / -inf_time_delta, "");
  // inf / 5 = inf
  EXPECT_EQ(inf, inf_time_delta / num);
  // inf / -5 = -inf
  EXPECT_EQ(-inf, inf_time_delta / -num);
  // -inf / 5 = -inf
  EXPECT_EQ(-inf, -inf_time_delta / num);
  // -inf / -5 = inf
  EXPECT_EQ(inf, -inf_time_delta / -num);
  // 5 / 0 = undefined
  EXPECT_DEATH_IF_SUPPORTED(num / zero, "");
  // 5 / inf = 0
  EXPECT_EQ(0, num / inf_time_delta);
  // 5 / -inf = 0
  EXPECT_EQ(0, num / -inf_time_delta);
  // 5 / 2 = 2.5
  EXPECT_EQ(2.5, num / ANIMATION_TIME_DELTA_FROM_SECONDS(2));
}

}  // namespace blink

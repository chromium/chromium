// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_time_delta.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <limits>

namespace blink {

TEST(AnimationTimeDeltaTest, Construction) {
  // The default constructor is a zero-length delta.
  EXPECT_EQ(AnimationTimeDelta(), AnimationTimeDelta::FromSecondsD(0));
  EXPECT_EQ(AnimationTimeDelta(), AnimationTimeDelta::FromMillisecondsD(0));

  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(5.5),
            AnimationTimeDelta::FromMillisecondsD(5500));
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(-2),
            AnimationTimeDelta::FromMillisecondsD(-2000));
}

TEST(AnimationTimeDeltaTest, Conversion) {
  AnimationTimeDelta delta = AnimationTimeDelta::FromSecondsD(5);
  EXPECT_EQ(delta.InSecondsF(), 5);
  EXPECT_EQ(delta.InMillisecondsF(), 5000);

  delta = AnimationTimeDelta::FromMillisecondsD(1234);
  EXPECT_EQ(delta.InSecondsF(), 1.234);
  EXPECT_EQ(delta.InMillisecondsF(), 1234);
}

TEST(AnimationTimeDeltaTest, Max) {
  AnimationTimeDelta max_delta = AnimationTimeDelta::Max();
  EXPECT_TRUE(max_delta.is_max());
  EXPECT_EQ(max_delta, AnimationTimeDelta::Max());
  EXPECT_GT(max_delta, AnimationTimeDelta::FromSecondsD(365 * 24 * 60 * 60));

  EXPECT_EQ(max_delta.InSecondsF(), std::numeric_limits<double>::infinity());
  EXPECT_EQ(max_delta.InMillisecondsF(),
            std::numeric_limits<double>::infinity());
}

TEST(AnimationTimeDeltaTest, Zero) {
  EXPECT_TRUE(AnimationTimeDelta().is_zero());
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(0).is_zero());
  EXPECT_TRUE(AnimationTimeDelta::FromMillisecondsD(0).is_zero());

  EXPECT_FALSE(AnimationTimeDelta::FromSecondsD(54.5).is_zero());
  EXPECT_FALSE(AnimationTimeDelta::FromSecondsD(-0.5).is_zero());
  EXPECT_FALSE(AnimationTimeDelta::FromMillisecondsD(123.45).is_zero());
}

TEST(AnimationTimeDeltaTest, Computation) {
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(4.5) +
                AnimationTimeDelta::FromMillisecondsD(500),
            AnimationTimeDelta::FromSecondsD(5));
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(100) +
                AnimationTimeDelta::FromMillisecondsD(-850),
            AnimationTimeDelta::FromSecondsD(99.15));

  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(5) * 20,
            AnimationTimeDelta::FromSecondsD(100));
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(10) * 1.5,
            AnimationTimeDelta::FromSecondsD(15));
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(2.5) * -2,
            AnimationTimeDelta::FromSecondsD(-5));

  EXPECT_EQ(20 * AnimationTimeDelta::FromSecondsD(5),
            AnimationTimeDelta::FromSecondsD(100));
}

TEST(AnimationTimeDeltaTest, Comparison) {
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(10) ==
              AnimationTimeDelta::FromSecondsD(10));
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(10) !=
              AnimationTimeDelta::FromSecondsD(50));
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(50) >
              AnimationTimeDelta::FromSecondsD(49.999));
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(50) >=
              AnimationTimeDelta::FromSecondsD(49.999));
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(50) >=
              AnimationTimeDelta::FromSecondsD(50));
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(50) <=
              AnimationTimeDelta::FromSecondsD(50));
  EXPECT_TRUE(AnimationTimeDelta::FromSecondsD(50) <=
              AnimationTimeDelta::FromSecondsD(100));
}

TEST(AnimationTimeDeltaTest, Division) {
  double inf = std::numeric_limits<double>::infinity();
  AnimationTimeDelta inf_time_delta = AnimationTimeDelta::Max();
  AnimationTimeDelta zero = AnimationTimeDelta();
  AnimationTimeDelta num = AnimationTimeDelta::FromSecondsD(5);

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
  EXPECT_EQ(2.5, num / AnimationTimeDelta::FromSecondsD(2));
}

}  // namespace blink

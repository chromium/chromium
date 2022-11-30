// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/frame_rate_estimator.h"

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using FpsPair = std::tuple<int, int>;

class FrameRateEstimatorTest : public testing::TestWithParam<FpsPair> {
 public:
  void ProvideSamples(base::TimeDelta duration, int count) {
    while (count--)
      estimator_.AddSample(duration);
  }

  void ProvideSample(base::TimeDelta duration) {
    estimator_.AddSample(duration);
  }

  int low_fps() const { return std::get<0>(GetParam()); }
  int high_fps() const { return std::get<1>(GetParam()); }

  base::TimeDelta duration(int fps) { return base::Seconds(1.0 / fps); }

  FrameRateEstimator estimator_;
};

TEST_P(FrameRateEstimatorTest, NoEstimateInitially) {
  // Asking for an estimate with no samples is okay, though it shouldn't return
  // an estimate.
  EXPECT_FALSE(estimator_.ComputeFPS());
}

TEST_P(FrameRateEstimatorTest, AverageConvergesThenReset) {
  // Verify that the estimate is provided after the required samples are reached
  // and that Reset() clears it.

  // The initial sample requirement should allow quick convergence.
  EXPECT_EQ(estimator_.GetRequiredSamplesForTesting(),
            estimator_.GetMinSamplesForTesting());

  // Make sure that it doesn't converge before the required sample count.
  ProvideSamples(duration(low_fps()),
                 estimator_.GetRequiredSamplesForTesting() - 1);
  EXPECT_FALSE(estimator_.ComputeFPS());
  ProvideSample(duration(low_fps()));
  EXPECT_EQ(*estimator_.ComputeFPS(), low_fps());

  estimator_.Reset();
  EXPECT_FALSE(estimator_.ComputeFPS());
  ProvideSamples(duration(low_fps()),
                 estimator_.GetRequiredSamplesForTesting() - 1);
  EXPECT_FALSE(estimator_.ComputeFPS());
}

TEST_P(FrameRateEstimatorTest, DurationJitterIsFine) {
  // A little jitter doesn't change anything.
  ProvideSamples(duration(low_fps()),
                 estimator_.GetRequiredSamplesForTesting());

  // Compute a jitter that's not big enough to move it out of its bucket.  We
  // use +1 so it works either above or below (below has more room).  2.0 would
  // be fine ideally, but we make it a bit smaller than that just to prevent
  // floating point weirdness.
  auto jitter = (duration(low_fps()) - duration(low_fps() + 1)) / 2.1;
  for (int i = 0; i < estimator_.GetRequiredSamplesForTesting(); i++) {
    ProvideSample(duration(low_fps()) + jitter);
    EXPECT_EQ(*estimator_.ComputeFPS(), low_fps());
  }

  for (int i = 0; i < estimator_.GetRequiredSamplesForTesting(); i++) {
    ProvideSample(duration(low_fps()) - jitter);
    EXPECT_EQ(*estimator_.ComputeFPS(), low_fps());
  }
}

TEST_P(FrameRateEstimatorTest, AverageDoesntSkew) {
  // Changing frame rates shouldn't skew between them.  It should stop providing
  // estimates temporarily.
  ProvideSamples(duration(low_fps()),
                 estimator_.GetRequiredSamplesForTesting());
  EXPECT_EQ(*estimator_.ComputeFPS(), low_fps());

  ProvideSample(duration(high_fps()));
  EXPECT_FALSE(estimator_.ComputeFPS());
  // We should now require more samples one we destabilized.
  EXPECT_EQ(estimator_.GetRequiredSamplesForTesting(),
            estimator_.GetMaxSamplesForTesting());
  ProvideSamples(duration(high_fps()),
                 estimator_.GetRequiredSamplesForTesting() - 2);
  EXPECT_FALSE(estimator_.ComputeFPS());
  ProvideSample(duration(high_fps()));
  EXPECT_EQ(*estimator_.ComputeFPS(), high_fps());
}

TEST_P(FrameRateEstimatorTest, ResetAllowsFastConvergence) {
  // If we're in slow-convergence mode, Reset() should allow fast convergence.

  // Get into slow convergence mode by providing a non-uniform window.
  ProvideSamples(duration(low_fps()), estimator_.GetMinSamplesForTesting() - 1);
  ProvideSamples(duration(high_fps()), 1);
  EXPECT_EQ(estimator_.GetRequiredSamplesForTesting(),
            estimator_.GetMaxSamplesForTesting());

  // See if Reset() gets us back to fast convergence.
  estimator_.Reset();
  EXPECT_EQ(estimator_.GetRequiredSamplesForTesting(),
            estimator_.GetMinSamplesForTesting());
}

// Instantiate tests for lots of common frame rates.
INSTANTIATE_TEST_SUITE_P(All,
                         FrameRateEstimatorTest,
                         testing::Values(FpsPair(24, 30),
                                         FpsPair(24, 60),
                                         FpsPair(24, 90),
                                         FpsPair(24, 120),
                                         FpsPair(24, 240),

                                         FpsPair(30, 60),
                                         FpsPair(30, 90),
                                         FpsPair(30, 120),
                                         FpsPair(30, 240),

                                         FpsPair(60, 90),
                                         FpsPair(60, 120),
                                         FpsPair(60, 240),

                                         FpsPair(90, 120),
                                         FpsPair(90, 240),

                                         FpsPair(120, 240)));

}  // namespace media

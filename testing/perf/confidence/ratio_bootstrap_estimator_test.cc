// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/perf/confidence/ratio_bootstrap_estimator.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(RatioBootstrapEstimatorTest, TrivialTest) {
  std::vector<RatioBootstrapEstimator::Sample> twice_as_fast = {
      {1.0, 0.5}, {1.01, 0.49}, {0.99, 0.51}};
  std::vector<RatioBootstrapEstimator::Sample> equally_fast = {
      {2.0, 2.01}, {2.01, 1.99}, {1.99, 2.0}};

  RatioBootstrapEstimator estimator(1111);
  std::vector<RatioBootstrapEstimator::Estimate> estimates =
      estimator.ComputeRatioEstimates({twice_as_fast, equally_fast}, 1000, 0.95,
                                      /*compute_geometric_mean=*/false);
  EXPECT_NEAR(2.0, estimates[0].lower, 0.2);
  EXPECT_NEAR(2.0, estimates[0].upper, 0.2);
  EXPECT_NEAR(2.0, estimates[0].point_estimate, 0.2);

  EXPECT_NEAR(1.0, estimates[1].lower, 0.2);
  EXPECT_NEAR(1.0, estimates[1].upper, 0.2);
  EXPECT_NEAR(1.0, estimates[1].point_estimate, 0.2);
}

TEST(RatioBootstrapEstimatorTest, GeometricMean) {
  std::vector<RatioBootstrapEstimator::Sample> twice_as_fast = {
      {1.0, 0.5}, {1.01, 0.49}, {0.99, 0.51}};
  std::vector<RatioBootstrapEstimator::Sample> equally_fast = {
      {2.0, 2.01}, {2.01, 1.99}, {1.99, 2.0}};

  RatioBootstrapEstimator estimator(2024);
  std::vector<RatioBootstrapEstimator::Estimate> estimates =
      estimator.ComputeRatioEstimates({twice_as_fast, equally_fast}, 1000, 0.95,
                                      /*compute_geometric_mean=*/true);
  EXPECT_NEAR(1.414, estimates[2].lower, 0.2);
  EXPECT_NEAR(1.414, estimates[2].upper, 0.2);
  EXPECT_NEAR(1.414, estimates[2].point_estimate, 0.2);
}

// This data set is picked out from a real Pinpoint run (a test of
// Speedometer3 Charts-chartjs) and checked with GNU R; we want
// to verify that we have approximately the same answer.
TEST(RatioBootstrapEstimatorTest, RealData) {
  std::vector<RatioBootstrapEstimator::Sample> data = {
      {{48.90, 50.14}, {48.76, 49.08}, {49.36, 50.10}, {50.49, 52.45},
       {49.65, 49.93}, {50.46, 49.14}, {50.82, 50.47}, {49.38, 48.72},
       {49.99, 49.33}, {50.02, 49.20}, {49.88, 49.53}, {50.21, 51.84},
       {51.42, 50.61}, {50.13, 50.08}, {54.62, 48.93}, {49.70, 49.29},
       {50.14, 50.20}, {49.91, 49.63}, {49.87, 49.17}, {49.15, 49.03},
       {49.78, 49.37}, {49.35, 51.32}, {51.57, 49.42}, {50.30, 50.80},
       {50.80, 50.24}, {49.92, 49.92}, {50.50, 49.77}, {49.92, 49.13},
       {50.45, 50.86}, {50.47, 50.83}, {49.75, 49.86}, {48.65, 52.53},
       {50.40, 49.61}, {48.74, 48.88}, {49.84, 49.37}, {48.51, 50.20},
       {48.81, 50.19}, {49.00, 51.06}, {51.25, 48.84}, {50.56, 50.38},
       {52.10, 48.96}, {51.10, 49.40}, {50.61, 50.01}, {49.08, 51.16},
       {49.48, 49.16}, {49.91, 50.25}, {49.66, 48.57}, {49.55, 50.41},
       {50.42, 51.98}, {49.44, 49.08}, {48.95, 49.77}, {49.87, 51.36},
       {50.64, 51.23}, {50.70, 48.51}, {49.06, 50.60}, {50.14, 50.25},
       {50.08, 52.62}, {48.81, 49.67}, {49.22, 50.78}, {49.30, 50.07},
       {48.93, 50.31}, {50.91, 51.90}, {50.59, 50.02}, {48.72, 52.83},
       {50.96, 48.52}, {48.61, 50.11}, {49.33, 49.58}, {49.43, 52.99},
       {48.94, 49.74}, {50.30, 49.54}, {50.24, 49.48}, {49.54, 49.69},
       {50.33, 50.11}, {49.94, 50.67}, {51.12, 50.95}, {50.36, 51.61},
       {48.99, 49.69}, {49.48, 50.45}, {48.95, 49.48}, {50.61, 50.74},
       {49.68, 49.69}, {50.38, 51.19}, {49.39, 50.80}, {49.16, 49.74},
       {49.54, 49.52}, {51.88, 49.33}, {49.22, 50.48}, {50.68, 50.15},
       {49.56, 48.88}, {50.11, 48.95}, {49.93, 49.54}, {49.74, 49.44},
       {49.09, 50.93}, {49.54, 49.53}, {51.15, 50.65}, {54.02, 50.31},
       {49.74, 49.82}, {49.76, 51.94}, {49.66, 49.56}, {49.97, 50.14},
       {48.71, 49.55}, {49.47, 51.33}, {49.64, 49.39}, {50.46, 49.66},
       {50.64, 49.53}, {49.69, 49.84}, {48.90, 49.82}, {50.58, 51.67},
       {49.53, 49.58}, {49.84, 50.52}, {50.57, 52.32}, {48.75, 51.78},
       {49.45, 49.71}, {49.77, 52.85}, {48.94, 48.92}, {50.49, 50.22},
       {51.06, 49.92}, {49.04, 49.72}, {49.00, 48.90}, {49.59, 49.25},
       {49.18, 50.21}, {53.87, 49.16}, {49.23, 49.84}, {49.84, 49.56},
       {49.33, 49.16}, {48.76, 50.03}, {50.19, 49.33}, {48.70, 49.47}}};

  RatioBootstrapEstimator estimator(1234);
  std::vector<RatioBootstrapEstimator::Estimate> estimates =
      estimator.ComputeRatioEstimates({data}, 10000, 0.95,
                                      /*compute_geometric_mean=*/false);

  // This data is higher-is-better, so we need to flip it (and thus
  // also swap higher/lower) to get throughput. R says [-0.2%, +0.8%]
  // at 95% CI.
  EXPECT_NEAR(-0.2, 100.0 * (1.0 / estimates[0].upper - 1.0), 0.1);
  EXPECT_NEAR(0.8, 100.0 * (1.0 / estimates[0].lower - 1.0), 0.1);

  // The point estimate should of course be deterministic, so we have lower
  // tolerance.
  EXPECT_NEAR(0.348, 100.0 * (1.0 / estimates[0].point_estimate - 1.0), 0.001);
}

TEST(RatioBootstrapEstimatorTest, InverseNormalCDF) {
  // Test values from the Wichura paper. (We use EXPECT_FLOAT_EQ
  // even though we have doubles, since we don't implement the most
  // accurate version. This precision is already overkill for us.)
  EXPECT_FLOAT_EQ(RatioBootstrapEstimator::InverseNormalCDF(0.25),
                  -0.674489750196081);
  EXPECT_FLOAT_EQ(RatioBootstrapEstimator::InverseNormalCDF(0.001),
                  -3.090232306167814);
  EXPECT_FLOAT_EQ(RatioBootstrapEstimator::InverseNormalCDF(1e-20),
                  -9.262340089798408);

  // A classic, from https://en.wikipedia.org/wiki/97.5th_percentile_point.
  EXPECT_FLOAT_EQ(RatioBootstrapEstimator::InverseNormalCDF(0.975),
                  1.95996398454005423552);
}

}  // namespace blink

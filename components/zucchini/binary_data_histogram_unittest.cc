// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/binary_data_histogram.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(OutlierDetectorTest, Basic) {
  auto make_detector = [](const std::vector<double>& values) {
    auto detector = std::make_unique<OutlierDetector>();
    for (double v : values)
      detector->Add(v);
    detector->Prepare();
    return detector;
  };

  std::unique_ptr<OutlierDetector> detector;
  // No data: Should at least not cause error.
  detector = make_detector({});
  EXPECT_EQ(0, detector->DecideOutlier(0.0));
  // Single point: Trivially inert.
  detector = make_detector({0.5});
  EXPECT_EQ(0, detector->DecideOutlier(0.1));
  EXPECT_EQ(0, detector->DecideOutlier(0.5));
  EXPECT_EQ(0, detector->DecideOutlier(0.9));
  // Two identical points: StdDev is 0, so falls back to built-in tolerance.
  detector = make_detector({0.5, 0.5});
  EXPECT_EQ(-1, detector->DecideOutlier(0.3));
  EXPECT_EQ(0, detector->DecideOutlier(0.499));
  EXPECT_EQ(0, detector->DecideOutlier(0.5));
  EXPECT_EQ(0, detector->DecideOutlier(0.501));
  EXPECT_EQ(1, detector->DecideOutlier(0.7));
  // Two separate points: Outliner test is pretty lax.
  detector = make_detector({0.4, 0.6});
  EXPECT_EQ(-1, detector->DecideOutlier(0.2));
  EXPECT_EQ(0, detector->DecideOutlier(0.3));
  EXPECT_EQ(0, detector->DecideOutlier(0.5));
  EXPECT_EQ(0, detector->DecideOutlier(0.7));
  EXPECT_EQ(1, detector->DecideOutlier(0.8));
  // Sharpen distribution by clustering toward norm: Now test is stricter.
  detector = make_detector({0.4, 0.47, 0.48, 0.49, 0.50, 0.51, 0.52, 0.6});
  EXPECT_EQ(-1, detector->DecideOutlier(0.3));
  EXPECT_EQ(0, detector->DecideOutlier(0.4));
  EXPECT_EQ(0, detector->DecideOutlier(0.5));
  EXPECT_EQ(0, detector->DecideOutlier(0.6));
  EXPECT_EQ(1, detector->DecideOutlier(0.7));
  // Shift numbers around: Mean is 0.3, and data order scrambled.
  detector = make_detector({0.28, 0.2, 0.31, 0.4, 0.29, 0.32, 0.27, 0.30});
  EXPECT_EQ(-1, detector->DecideOutlier(0.0));
  EXPECT_EQ(-1, detector->DecideOutlier(0.1));
  EXPECT_EQ(0, detector->DecideOutlier(0.2));
  EXPECT_EQ(0, detector->DecideOutlier(0.3));
  EXPECT_EQ(0, detector->DecideOutlier(0.4));
  EXPECT_EQ(1, detector->DecideOutlier(0.5));
  EXPECT_EQ(1, detector->DecideOutlier(1.0));
  // Typical usage: Potential outlier would be part of original input data!
  detector = make_detector({0.3, 0.29, 0.31, 0.0, 0.3, 0.32, 0.3, 0.29, 0.6});
  EXPECT_EQ(-1, detector->DecideOutlier(0.0));
  EXPECT_EQ(0, detector->DecideOutlier(0.28));
  EXPECT_EQ(0, detector->DecideOutlier(0.29));
  EXPECT_EQ(0, detector->DecideOutlier(0.3));
  EXPECT_EQ(0, detector->DecideOutlier(0.31));
  EXPECT_EQ(0, detector->DecideOutlier(0.32));
  EXPECT_EQ(1, detector->DecideOutlier(0.6));
}

TEST(BinaryDataHistogramTest, Basic) {
  constexpr double kUninitScore = -1;

  constexpr uint8_t kTestData[] = {2, 137, 42, 0, 0, 0, 7, 11, 1, 11, 255};
  const size_t n = sizeof(kTestData);
  ConstBufferView region(kTestData, n);

  std::vector<BinaryDataHistogram> prefix_histograms(n + 1);  // Short to long.
  std::vector<BinaryDataHistogram> suffix_histograms(n + 1);  // Long to short.

  for (size_t i = 0; i <= n; ++i) {
    ConstBufferView prefix(region.begin(), i);
    ConstBufferView suffix(region.begin() + i, n - i);
    // If regions are smaller than 2 bytes then it is invalid. Else valid.
    EXPECT_EQ(prefix.size() >= 2, prefix_histograms[i].Compute(prefix));
    EXPECT_EQ(suffix.size() >= 2, suffix_histograms[i].Compute(suffix));
    // IsValid() returns the same results.
    EXPECT_EQ(prefix.size() >= 2, prefix_histograms[i].IsValid());
    EXPECT_EQ(suffix.size() >= 2, suffix_histograms[i].IsValid());
  }

  // Full-prefix = full-suffix = full data.
  EXPECT_EQ(0.0, prefix_histograms[n].Distance(suffix_histograms[0]));
  EXPECT_EQ(0.0, suffix_histograms[0].Distance(prefix_histograms[n]));

  // Testing heuristics without overreliance on implementation details.

  // Strict prefixes, in increasing size. Compare against full data.
  double prev_prefix_score = kUninitScore;
  for (size_t i = 2; i < n; ++i) {
    double score = prefix_histograms[i].Distance(prefix_histograms[n]);
    // Positivity.
    EXPECT_GT(score, 0.0);
    // Symmetry.
    EXPECT_EQ(score, prefix_histograms[n].Distance(prefix_histograms[i]));
    // Distance should decrease as prefix gets nearer to full data.
    if (prev_prefix_score != kUninitScore)
      EXPECT_LT(score, prev_prefix_score);
    prev_prefix_score = score;
  }

  // Strict suffixes, in decreasing size. Compare against full data.
  double prev_suffix_score = -1;
  for (size_t i = 1; i <= n - 2; ++i) {
    double score = suffix_histograms[i].Distance(suffix_histograms[0]);
    // Positivity.
    EXPECT_GT(score, 0.0);
    // Symmetry.
    EXPECT_EQ(score, suffix_histograms[0].Distance(suffix_histograms[i]));
    // Distance should increase as suffix gets farther from full data.
    if (prev_suffix_score != kUninitScore)
      EXPECT_GT(score, prev_suffix_score);
    prev_suffix_score = score;
  }
}

}  // namespace zucchini

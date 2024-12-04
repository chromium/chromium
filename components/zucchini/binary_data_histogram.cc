// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/binary_data_histogram.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace zucchini {

/******** OutlierDetector ********/

OutlierDetector::OutlierDetector() = default;

OutlierDetector::~OutlierDetector() = default;

// For BinaryDataHistogram, |sample| is typically in interval [0, 1].
void OutlierDetector::Add(double sample) {
  ++n_;
  sum_ += sample;
  sum_of_squares_ += sample * sample;
}

void OutlierDetector::Prepare() {
  if (n_ > 0) {
    mean_ = sum_ / n_;
    standard_deviation_ = ::sqrt((sum_of_squares_ - sum_ * mean_) /
                                 std::max(static_cast<size_t>(1), n_ - 1));
  }
}

std::string OutlierDetector::RenderStats() {
  return base::StringPrintf("Mean = %.5f, StdDev = %.5f over %" PRIuS
                            " samples",
                            mean_, standard_deviation_, n_);
}

// Constants are chosen for BinaryDataHistogram, where |sample| is typically in
// [0, 1].
int OutlierDetector::DecideOutlier(double sample) {
  // Lower bound to avoid divide-by-zero and penalizing tight clusters.
  constexpr double kMinTolerance = 0.1;
  // Number of standard deviations away from mean for value to become outlier.
  constexpr double kSigmaBound = 1.9;
  if (n_ <= 1)
    return 0;
  double tolerance = std::max(kMinTolerance, standard_deviation_);
  double num_sigma = (sample - mean_) / tolerance;
  return num_sigma > kSigmaBound ? 1 : num_sigma < -kSigmaBound ? -1 : 0;
}

/******** BinaryDataHistogram ********/

BinaryDataHistogram::BinaryDataHistogram() = default;

BinaryDataHistogram::~BinaryDataHistogram() = default;

bool BinaryDataHistogram::Compute(ConstBufferView region) {
  DCHECK(!histogram_);
  // Binary data with size < 2 are invalid.
  if (region.size() < sizeof(uint16_t))
    return false;
  DCHECK_LE(region.size(),
            static_cast<size_t>(std::numeric_limits<int32_t>::max()));

  histogram_ = std::make_unique<int32_t[]>(kNumBins);
  size_ = region.size();
  // Number of 2-byte intervals fully contained in |region|.
  size_t bound = size_ - sizeof(uint16_t) + 1;
  for (size_t i = 0; i < bound; ++i)
    ++histogram_[region.read<uint16_t>(i)];
  return true;
}

double BinaryDataHistogram::Distance(const BinaryDataHistogram& other) const {
  DCHECK(IsValid() && other.IsValid());
  // Compute Manhattan (L1) distance between respective histograms.
  double total_diff = 0;
  for (int i = 0; i < kNumBins; ++i)
    total_diff += std::abs(histogram_[i] - other.histogram_[i]);
  // Normalize by total size, so result lies in [0, 1].
  return total_diff / (size_ + other.size_);
}

}  // namespace zucchini

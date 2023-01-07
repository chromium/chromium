// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_BINARY_DATA_HISTOGRAM_H_
#define COMPONENTS_ZUCCHINI_BINARY_DATA_HISTOGRAM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "components/zucchini/buffer_view.h"

namespace zucchini {

// A class to detect outliers in a list of doubles using Chauvenet's criterion:
// Compute mean and standard deviation of observations, then determine whether
// a query value lies beyond a fixed number of standard deviations (sigmas) from
// the mean. The purpose of this test is to reduce the chance of false-positive
// ensemble matches.
class OutlierDetector {
 public:
  OutlierDetector();
  OutlierDetector(const OutlierDetector&) = delete;
  const OutlierDetector& operator=(const OutlierDetector&) = delete;
  ~OutlierDetector();

  // Incorporates |sample| into mean and standard deviation.
  void Add(double sample);

  // Prepares basic statistics for DecideOutlier() calls. Should be called after
  // all samples have been added.
  void Prepare();

  // Renders current statistics as strings for logging.
  std::string RenderStats();

  // Heuristically decides whether |sample| is an outlier. Returns 1 if |sample|
  // is "too high", 0 if |sample| is "normal", and -1 if |sample| is "too low".
  // Must be called after Prepare().
  int DecideOutlier(double sample);

 private:
  size_t n_ = 0;
  double sum_ = 0;
  double sum_of_squares_ = 0;
  double mean_ = 0;
  double standard_deviation_ = 0;
};

// A class to compute similarity score between binary data. The heuristic here
// preprocesses input data to a size-65536 histogram, counting the frequency of
// consecutive 2-byte sequences. Therefore data with lengths < 2 are considered
// invalid -- but this is okay for Zucchini's use case.
class BinaryDataHistogram {
 public:
  BinaryDataHistogram();
  BinaryDataHistogram(const BinaryDataHistogram&) = delete;
  const BinaryDataHistogram& operator=(const BinaryDataHistogram&) = delete;
  ~BinaryDataHistogram();

  // Attempts to compute the histogram, returns true iff successful.
  bool Compute(ConstBufferView region);

  bool IsValid() const { return static_cast<bool>(histogram_); }

  // Returns distance to another histogram (heuristics). If two binaries are
  // identical then their histogram distance is 0. However, the converse is not
  // true in general. For example, "aba" and "bab" are different, but their
  // histogram distance is 0 (both histograms are {"ab": 1, "ba": 1}).
  double Distance(const BinaryDataHistogram& other) const;

 private:
  enum { kNumBins = 1 << (sizeof(uint16_t) * 8) };
  static_assert(kNumBins == 65536, "Incorrect constant computation.");

  // Size, in bytes, of the data over which the histogram was computed.
  size_t size_ = 0;

  // 2^16 buckets holding counts of all 2-byte sequences in the data. The counts
  // are stored as signed values to simplify computing the distance between two
  // histograms.
  std::unique_ptr<int32_t[]> histogram_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_BINARY_DATA_HISTOGRAM_H_

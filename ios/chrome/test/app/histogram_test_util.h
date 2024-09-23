// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_HISTOGRAM_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_HISTOGRAM_TEST_UTIL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"

// TODO(crbug.com/41271490): factorize with base::HistogramTester and add
// unittests.

typedef void (^FailureBlock)(NSString*);

namespace base {
class HistogramSamples;
}

namespace chrome_test_util {

struct Bucket;

// HistogramTestUtil provides a simple interface for examining histograms, UMA
// or otherwise. Earl Grey tests can use this interface to verify that histogram
// data is getting logged as intended.
class HistogramTester {
 public:
  using CountsMap = std::map<std::string, base::HistogramBase::Count>;

  // Takes a snapshot of all current histograms counts.
  HistogramTester();

  HistogramTester(const HistogramTester&) = delete;
  HistogramTester& operator=(const HistogramTester&) = delete;

  ~HistogramTester();

  // We know the exact number of samples in a bucket, and that no other bucket
  // should have samples. Measures the diff from the snapshot taken when this
  // object was constructed.
  // Returns true if the bucket contains `expected_count` samples and no other
  // buckets have samples. If not, call `failure_block` with a descriptive text
  // of the error.
  BOOL ExpectUniqueSample(const std::string& name,
                          base::HistogramBase::Sample sample,
                          base::HistogramBase::Count expected_count,
                          FailureBlock failure_block) const;

  // We know the exact number of samples in a bucket, but other buckets may
  // have samples as well. Measures the diff from the snapshot taken when this
  // object was constructed.
  // Returns true if the bucket contains `expected_count` samples. If not, call
  // `failure_block` with a descriptive text of the error.
  BOOL ExpectBucketCount(const std::string& name,
                         base::HistogramBase::Sample sample,
                         base::HistogramBase::Count expected_count,
                         FailureBlock failure_block) const;

  // We don't know the values of the samples, but we know how many there are.
  // This measures the diff from the snapshot taken when this object was
  // constructed.
  // Returns true if the histogram contains `count` samples. If not, call
  // `failure_block` with a descriptive text of the error.
  BOOL ExpectTotalCount(const std::string& name,
                        base::HistogramBase::Count count,
                        FailureBlock failure_block) const;

  // Returns a list of all of the buckets recorded since creation of this
  // object, as vector<Bucket>, where a Bucket represents the min boundary of
  // the bucket and the count of samples recorded to that bucket since creation.
  // If there is not histogram named `name`, return an empty vector.
  std::vector<Bucket> GetAllSamples(const std::string& name) const;

  // Returns a modified HistogramSamples containing only what has been logged
  // to the histogram since the creation of this object. Returns an empty vector
  // if the histogram is not found.
  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceCreation(
      const std::string& histogram_name) const;

 private:
  // Verifies and asserts that value in the `sample` bucket matches the
  // `expected_count`. The bucket's current value is determined from `samples`
  // and is modified based on the snapshot stored for histogram `name`.
  BOOL CheckBucketCount(const std::string& name,
                        base::HistogramBase::Sample sample,
                        base::Histogram::Count expected_count,
                        const base::HistogramSamples& samples,
                        FailureBlock failure_block) const;

  // Verifies that the total number of values recorded for the histogram `name`
  // is `expected_count`. This is checked against `samples` minus the snapshot
  // that was taken for `name`.
  BOOL CheckTotalCount(const std::string& name,
                       base::Histogram::Count expected_count,
                       const base::HistogramSamples& samples,
                       FailureBlock failure_block) const;

  // Used to determine the histogram changes made during this instance's
  // lifecycle. This instance takes ownership of the samples, which are deleted
  // when the instance is destroyed.
  std::map<std::string, std::unique_ptr<base::HistogramSamples>>
      histograms_snapshot_;
};

struct Bucket {
  Bucket(base::HistogramBase::Sample min, base::HistogramBase::Count count)
      : min(min), count(count) {}

  bool operator==(const Bucket& other) const;

  base::HistogramBase::Sample min;
  base::HistogramBase::Count count;
};

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_HISTOGRAM_TEST_UTIL_H_

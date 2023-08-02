// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/histogram_test_util.h"

#import <Foundation/Foundation.h>

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/histogram_samples.h"
#import "base/metrics/metrics_hashes.h"
#import "base/metrics/sample_map.h"
#import "base/metrics/statistics_recorder.h"

namespace {
base::HistogramBase* FindHistogram(const std::string& name,
                                   FailureBlock failure_block) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram && failure_block) {
    failure_block([NSString
        stringWithFormat:@"Histogram %s does not exist", name.c_str()]);
  }
  return histogram;
}
}  // namespace

namespace chrome_test_util {

HistogramTester::HistogramTester() {
  // Record any histogram data that exists when the object is created so it can
  // be subtracted later.
  for (const auto* const h : base::StatisticsRecorder::GetHistograms()) {
    histograms_snapshot_[h->histogram_name()] = h->SnapshotSamples();
  }
}

HistogramTester::~HistogramTester() {
  histograms_snapshot_.clear();
}

BOOL HistogramTester::ExpectUniqueSample(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count expected_count,
    FailureBlock failure_block) const {
  base::HistogramBase* histogram = FindHistogram(name, failure_block);
  if (!histogram) {
    return NO;
  }

  std::unique_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
  if (!CheckBucketCount(name, sample, expected_count, *samples,
                        failure_block)) {
    return NO;
  }
  if (!CheckTotalCount(name, expected_count, *samples, failure_block)) {
    return NO;
  }
  return YES;
}

BOOL HistogramTester::ExpectBucketCount(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count expected_count,
    FailureBlock failure_block) const {
  BOOL not_found_fails = expected_count > 0;
  FailureBlock not_found_block =
      not_found_fails ? failure_block : static_cast<FailureBlock>(nil);
  base::HistogramBase* histogram = FindHistogram(name, not_found_block);
  if (!histogram) {
    return !not_found_fails;
  }

  std::unique_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
  return CheckBucketCount(name, sample, expected_count, *samples,
                          failure_block);
}

BOOL HistogramTester::ExpectTotalCount(const std::string& name,
                                       base::HistogramBase::Count count,
                                       FailureBlock failure_block) const {
  BOOL not_found_fails = count > 0;
  FailureBlock not_found_block =
      not_found_fails ? failure_block : static_cast<FailureBlock>(nil);
  base::HistogramBase* histogram = FindHistogram(name, not_found_block);
  if (!histogram) {
    return !not_found_fails;
  }
  std::unique_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
  return CheckTotalCount(name, count, *samples, failure_block);
}

std::vector<Bucket> HistogramTester::GetAllSamples(
    const std::string& name) const {
  std::vector<Bucket> samples;
  std::unique_ptr<base::HistogramSamples> snapshot =
      GetHistogramSamplesSinceCreation(name);
  if (snapshot) {
    for (auto it = snapshot->Iterator(); !it->Done(); it->Next()) {
      base::HistogramBase::Sample sample;
      base::HistogramBase::Count count;
      it->Get(&sample, nullptr, &count);
      samples.push_back(Bucket(sample, count));
    }
  }
  return samples;
}

std::unique_ptr<base::HistogramSamples>
HistogramTester::GetHistogramSamplesSinceCreation(
    const std::string& histogram_name) const {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  // Whether the histogram exists or not may not depend on the current test
  // calling this method, but rather on which tests ran before and possibly
  // generated a histogram or not (see http://crbug.com/473689). To provide a
  // response which is independent of the previously run tests, this method
  // creates empty samples in the absence of the histogram, rather than
  // returning null.
  if (!histogram) {
    return std::unique_ptr<base::HistogramSamples>(
        new base::SampleMap(base::HashMetricName(histogram_name)));
  }
  std::unique_ptr<base::HistogramSamples> named_samples(
      histogram->SnapshotSamples());
  auto original_samples_it = histograms_snapshot_.find(histogram_name);
  if (original_samples_it != histograms_snapshot_.end())
    named_samples->Subtract(*original_samples_it->second);
  return named_samples;
}

BOOL HistogramTester::CheckBucketCount(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count expected_count,
    const base::HistogramSamples& samples,
    FailureBlock failure_block) const {
  int actual_count = samples.GetCount(sample);
  auto histogram_data = histograms_snapshot_.find(name);
  if (histogram_data != histograms_snapshot_.end())
    actual_count -= histogram_data->second->GetCount(sample);
  if (expected_count == actual_count) {
    return YES;
  }
  if (failure_block) {
    failure_block([NSString
        stringWithFormat:
            @"Histogram \"%s\" does not have the "
             "right number of samples(%d) in the expected bucket(%d). It has "
             "(%d).",
            name.c_str(), expected_count, sample, actual_count]);
  }
  return NO;
}

BOOL HistogramTester::CheckTotalCount(const std::string& name,
                                      base::HistogramBase::Count expected_count,
                                      const base::HistogramSamples& samples,
                                      FailureBlock failure_block) const {
  int actual_count = samples.TotalCount();
  auto histogram_data = histograms_snapshot_.find(name);
  if (histogram_data != histograms_snapshot_.end())
    actual_count -= histogram_data->second->TotalCount();
  if (expected_count == actual_count) {
    return YES;
  }
  if (failure_block) {
    failure_block(
        [NSString stringWithFormat:@"Histogram \"%s\" does not have the "
                                    "right total number of samples(%d). It has "
                                    "(%d).",
                                   name.c_str(), expected_count, actual_count]);
  }
  return NO;
}

}  // namespace chrome_test_util

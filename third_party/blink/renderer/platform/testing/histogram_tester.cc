// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"

namespace blink {

HistogramTester::HistogramTester()
    : histogram_tester_(std::make_unique<base::HistogramTester>()) {}

HistogramTester::~HistogramTester() = default;

void HistogramTester::ExpectUniqueSample(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count count) const {
  histogram_tester_->ExpectUniqueSample(name, sample, count);
}

void HistogramTester::ExpectBucketCount(
    const std::string& name,
    base::HistogramBase::Sample sample,
    base::HistogramBase::Count count) const {
  histogram_tester_->ExpectBucketCount(name, sample, count);
}

void HistogramTester::ExpectTotalCount(const std::string& name,
                                       base::HistogramBase::Count count) const {
  histogram_tester_->ExpectTotalCount(name, count);
}

base::HistogramBase::Count HistogramTester::GetBucketCount(
    const std::string& name,
    base::HistogramBase::Sample sample) const {
  return histogram_tester_->GetBucketCount(name, sample);
}

}  // namespace blink

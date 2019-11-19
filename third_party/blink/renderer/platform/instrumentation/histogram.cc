// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/numerics/safe_conversions.h"

namespace blink {

CustomCountHistogram::CustomCountHistogram(const char* name,
                                           base::HistogramBase::Sample min,
                                           base::HistogramBase::Sample max,
                                           int32_t bucket_count) {
  histogram_ = base::Histogram::FactoryGet(
      name, min, max, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

CustomCountHistogram::CustomCountHistogram(base::HistogramBase* histogram)
    : histogram_(histogram) {}

void CustomCountHistogram::Count(base::HistogramBase::Sample sample) {
  histogram_->Add(sample);
}

void CustomCountHistogram::CountMany(base::HistogramBase::Sample sample,
                                     int count) {
  histogram_->AddCount(sample, count);
}

void CustomCountHistogram::CountMicroseconds(base::TimeDelta delta) {
  Count(base::saturated_cast<base::HistogramBase::Sample>(
      delta.InMicroseconds()));
}

void CustomCountHistogram::CountMilliseconds(base::TimeDelta delta) {
  Count(base::saturated_cast<base::HistogramBase::Sample>(
      delta.InMilliseconds()));
}

BooleanHistogram::BooleanHistogram(const char* name)
    : CustomCountHistogram(base::BooleanHistogram::FactoryGet(
          name,
          base::HistogramBase::kUmaTargetedHistogramFlag)) {}

EnumerationHistogram::EnumerationHistogram(
    const char* name,
    base::HistogramBase::Sample boundary_value)
    : CustomCountHistogram(base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag)) {}

SparseHistogram::SparseHistogram(const char* name) {
  histogram_ = base::SparseHistogram::FactoryGet(
      name, base::HistogramBase::kUmaTargetedHistogramFlag);
}

void SparseHistogram::Sample(base::HistogramBase::Sample sample) {
  histogram_->Add(sample);
}

LinearHistogram::LinearHistogram(const char* name,
                                 base::HistogramBase::Sample min,
                                 base::HistogramBase::Sample max,
                                 int32_t bucket_count)
    : CustomCountHistogram(base::LinearHistogram::FactoryGet(
          name,
          min,
          max,
          bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag)) {}

}  // namespace blink

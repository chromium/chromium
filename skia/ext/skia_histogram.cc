// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_histogram.h"

#include <type_traits>

#include "base/metrics/histogram_macros.h"

namespace skia {

// Wrapper around HISTOGRAM_POINTER_USE - mimics UMA_HISTOGRAM_BOOLEAN but
// allows for an external atomic_histogram_pointer.
void HistogramBoolean(std::atomic_uintptr_t* atomic_histogram_pointer,
                      const char* name,
                      bool sample) {
  HISTOGRAM_POINTER_USE(
      atomic_histogram_pointer, name, AddBoolean(sample),
      base::BooleanHistogram::FactoryGet(
          name, base::HistogramBase::kUmaTargetedHistogramFlag));
}

// Wrapper around HISTOGRAM_POINTER_USE - mimics UMA_HISTOGRAM_ENUMERATION but
// allows for an external atomic_histogram_pointer.
void HistogramEnumeration(std::atomic_uintptr_t* atomic_histogram_pointer,
                          const char* name,
                          int sample,
                          int enum_size) {
  HISTOGRAM_POINTER_USE(atomic_histogram_pointer, name, Add(sample),
                        base::LinearHistogram::FactoryGet(
                            name, 1, enum_size, enum_size + 1,
                            base::HistogramBase::kUmaTargetedHistogramFlag));
}

// Wrapper around HISTOGRAM_POINTER_USE - mimics UMA_HISTOGRAM_EXACT_LINEAR but
// allows for an external atomic_histogram_pointer.
void HistogramExactLinear(std::atomic_uintptr_t* atomic_histogram_pointer,
                          const char* name,
                          int sample,
                          int value_max) {
  HISTOGRAM_POINTER_USE(atomic_histogram_pointer, name, Add(sample),
                        base::LinearHistogram::FactoryGet(
                            name, 1, value_max, value_max + 1,
                            base::HistogramBase::kUmaTargetedHistogramFlag));
}

// Wrapper around HISTOGRAM_POINTER_USE - mimics UMA_HISTOGRAM_EXACT_LINEAR but
// allows for an external atomic_histogram_pointer.
void HistogramCustomExactLinear(std::atomic_uintptr_t* atomic_histogram_pointer,
                                const char* name,
                                int sample,
                                int value_min,
                                int value_max,
                                size_t bucket_count) {
  HISTOGRAM_POINTER_USE(atomic_histogram_pointer, name, Add(sample),
                        base::LinearHistogram::FactoryGet(
                            name, value_min, value_max, bucket_count,
                            base::HistogramBase::kUmaTargetedHistogramFlag));
}

// Wrapper around HISTOGRAM_POINTER_USE - mimics UMA_HISTOGRAM_MEMORY_KB but
// allows for an external atomic_histogram_pointer.
void HistogramMemoryKB(std::atomic_uintptr_t* atomic_histogram_pointer,
                       const char* name,
                       int sample) {
  HISTOGRAM_POINTER_USE(atomic_histogram_pointer, name, Add(sample),
                        base::Histogram::FactoryGet(
                            name, 1000, 500000, 50,
                            base::HistogramBase::kUmaTargetedHistogramFlag));
}

// Wrapper around HISTOGRAM_POINTER_USE - mimics
// UMA_HISTOGRAM_CUSTOM_COUNTS but allows for an external
// atomic_histogram_pointer.
void HistogramCustomCounts(std::atomic_uintptr_t* atomic_histogram_pointer,
                           const char* name,
                           int sample,
                           int min_count,
                           int max_count,
                           size_t bucket_count) {
  HISTOGRAM_POINTER_USE(atomic_histogram_pointer, name, Add(sample),
                        base::Histogram::FactoryGet(
                            name, min_count, max_count, bucket_count,
                            base::HistogramBase::kUmaTargetedHistogramFlag));
}

// Wrapper around HISTOGRAM_POINTER_USE - mimics
// UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES but allows for an external
// atomic_histogram_pointer.
void HistogramCustomMicrosecondsTimes(
    std::atomic_uintptr_t* atomic_histogram_pointer,
    const char* name,
    int64_t sample_usec,
    unsigned min_usec,
    unsigned max_usec,
    size_t bucket_count) {
  HISTOGRAM_POINTER_USE(
      atomic_histogram_pointer, name, Add(sample_usec),
      base::Histogram::FactoryMicrosecondsTimeGet(
          name, base::Microseconds(min_usec), base::Microseconds(max_usec),
          bucket_count, base::HistogramBase::kUmaTargetedHistogramFlag));
}

}  // namespace skia

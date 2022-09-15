// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_HISTOGRAM_H_
#define SKIA_EXT_SKIA_HISTOGRAM_H_

#include <stdint.h>

#include <atomic>
#include <memory>

// This file exposes Chrome's histogram functionality to Skia, without bringing
// in any Chrome specific headers. To achieve the same level of optimization as
// is present in Chrome, we need to use an inlined atomic pointer. This macro
// defines a placeholder atomic which will be inlined into the call-site. This
// placeholder is passed to the actual histogram logic in Chrome.
#define SK_HISTOGRAM_POINTER_HELPER(function, ...)                   \
  do {                                                               \
    static std::atomic_uintptr_t atomic_histogram_pointer;           \
    function(std::addressof(atomic_histogram_pointer), __VA_ARGS__); \
  } while (0)

#define SK_HISTOGRAM_BOOLEAN(name, sample) \
  SK_HISTOGRAM_POINTER_HELPER(skia::HistogramBoolean, "Skia." name, sample)

#define SK_HISTOGRAM_EXACT_LINEAR(name, sample, value_max)              \
  SK_HISTOGRAM_POINTER_HELPER(skia::HistogramExactLinear, "Skia." name, \
                              sample, value_max)

#define SK_HISTOGRAM_MEMORY_KB(name, sample) \
  SK_HISTOGRAM_POINTER_HELPER(skia::HistogramMemoryKB, "Skia." name, sample)

namespace skia {

void HistogramBoolean(std::atomic_uintptr_t* atomic_histogram_pointer,
                      const char* name,
                      bool sample);
void HistogramExactLinear(std::atomic_uintptr_t* atomic_histogram_pointer,
                          const char* name,
                          int sample,
                          int value_max);
void HistogramMemoryKB(std::atomic_uintptr_t* atomic_histogram_pointer,
                       const char* name,
                       int sample);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_HISTOGRAM_H_

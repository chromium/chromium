// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CLIENT_STATS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CLIENT_STATS_IMPL_H_

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace quiche {

// By convention, all QUIC histograms are prefixed by "Net.".
#define QUICHE_HISTOGRAM_NAME(raw_name) "Net." raw_name

#define QUICHE_CLIENT_HISTOGRAM_ENUM_IMPL(name, sample, enum_size, docstring) \
  UMA_HISTOGRAM_ENUMERATION(QUICHE_HISTOGRAM_NAME(name), sample, enum_size)

#define QUICHE_CLIENT_HISTOGRAM_BOOL_IMPL(name, sample, docstring) \
  UMA_HISTOGRAM_BOOLEAN(QUICHE_HISTOGRAM_NAME(name), sample)

#define QUICHE_CLIENT_HISTOGRAM_TIMES_IMPL(name, sample, min, max,        \
                                           bucket_count, docstring)       \
  UMA_HISTOGRAM_CUSTOM_TIMES(QUICHE_HISTOGRAM_NAME(name),                 \
                             base::Microseconds(sample.ToMicroseconds()), \
                             base::Microseconds(min.ToMicroseconds()),    \
                             base::Microseconds(max.ToMicroseconds()),    \
                             bucket_count)

#define QUICHE_CLIENT_HISTOGRAM_COUNTS_IMPL(name, sample, min, max,          \
                                            bucket_count, docstring)         \
  UMA_HISTOGRAM_CUSTOM_COUNTS(QUICHE_HISTOGRAM_NAME(name), sample, min, max, \
                              bucket_count)

inline void QuicheClientSparseHistogramImpl(const std::string& name,
                                            int sample) {
  base::UmaHistogramSparse(name, sample);
}

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CLIENT_STATS_IMPL_H_

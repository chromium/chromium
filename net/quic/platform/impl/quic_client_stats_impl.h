// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_CLIENT_STATS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_CLIENT_STATS_IMPL_H_

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace quic {

// By convention, all QUIC histograms are prefixed by "Net.".
#define QUIC_HISTOGRAM_NAME(raw_name) "Net." raw_name

#define QUIC_CLIENT_HISTOGRAM_ENUM_IMPL(name, sample, enum_size, docstring) \
  UMA_HISTOGRAM_ENUMERATION(QUIC_HISTOGRAM_NAME(name), sample, enum_size)

#define QUIC_CLIENT_HISTOGRAM_BOOL_IMPL(name, sample, docstring) \
  UMA_HISTOGRAM_BOOLEAN(QUIC_HISTOGRAM_NAME(name), sample)

#define QUIC_CLIENT_HISTOGRAM_TIMES_IMPL(name, sample, min, max, bucket_count, \
                                         docstring)                            \
  UMA_HISTOGRAM_CUSTOM_TIMES(                                                  \
      QUIC_HISTOGRAM_NAME(name),                                               \
      base::TimeDelta::FromMicroseconds(sample.ToMicroseconds()),              \
      base::TimeDelta::FromMicroseconds(min.ToMicroseconds()),                 \
      base::TimeDelta::FromMicroseconds(max.ToMicroseconds()), bucket_count)

#define QUIC_CLIENT_HISTOGRAM_COUNTS_IMPL(name, sample, min, max,          \
                                          bucket_count, docstring)         \
  UMA_HISTOGRAM_CUSTOM_COUNTS(QUIC_HISTOGRAM_NAME(name), sample, min, max, \
                              bucket_count)

inline void QuicClientSparseHistogramImpl(const std::string& name, int sample) {
  base::UmaHistogramSparse(name, sample);
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_CLIENT_STATS_IMPL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_SERVER_STATS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_SERVER_STATS_IMPL_H_

namespace quic {

#define QUIC_SERVER_HISTOGRAM_ENUM_IMPL(name, sample, enum_size, docstring) \
  do {                                                                      \
  } while (0)

#define QUIC_SERVER_HISTOGRAM_BOOL_IMPL(name, sample, docstring) \
  do {                                                           \
  } while (0)

#define QUIC_SERVER_HISTOGRAM_TIMES_IMPL(name, sample, min, max, bucket_count, \
                                         docstring)                            \
  do {                                                                         \
  } while (0)

#define QUIC_SERVER_HISTOGRAM_COUNTS_IMPL(name, sample, min, max,  \
                                          bucket_count, docstring) \
  do {                                                             \
  } while (0)

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_SERVER_STATS_IMPL_H_

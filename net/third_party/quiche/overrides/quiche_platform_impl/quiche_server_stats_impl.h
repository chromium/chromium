// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SERVER_STATS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SERVER_STATS_IMPL_H_

#define QUICHE_SERVER_HISTOGRAM_ENUM_IMPL(name, sample, enum_size, docstring) \
  do {                                                                        \
  } while (0)

#define QUICHE_SERVER_HISTOGRAM_BOOL_IMPL(name, sample, docstring) \
  do {                                                             \
  } while (0)

#define QUICHE_SERVER_HISTOGRAM_TIMES_IMPL(name, sample, min, max,  \
                                           bucket_count, docstring) \
  do {                                                              \
  } while (0)

#define QUICHE_SERVER_HISTOGRAM_COUNTS_IMPL(name, sample, min, max,  \
                                            bucket_count, docstring) \
  do {                                                               \
  } while (0)

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SERVER_STATS_IMPL_H_

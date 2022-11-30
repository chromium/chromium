// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_TIMING_INFO_TEST_UTIL_H_
#define NET_BASE_LOAD_TIMING_INFO_TEST_UTIL_H_

#include "net/base/load_timing_info.h"

namespace net {

// Flags indicating which times in a LoadTimingInfo::ConnectTiming struct should
// be non-null.
enum ConnectTimeFlags {
  CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY = 0,
  CONNECT_TIMING_HAS_DNS_TIMES          = 1 << 0,
  CONNECT_TIMING_HAS_SSL_TIMES          = 1 << 1,
};

// Checks that all times in |connect_timing| are null.
void ExpectConnectTimingHasNoTimes(
    const LoadTimingInfo::ConnectTiming& connect_timing);

// Checks that |connect_timing|'s times are in the correct order.
// |connect_start| and |connect_end| must be non-null.  Checks null state and
// order of DNS times and SSL times based on |flags|, which must be a
// combination of ConnectTimeFlags.
void ExpectConnectTimingHasTimes(
    const LoadTimingInfo::ConnectTiming& connect_timing,
    int connect_timing_flags);

// Tests that all non-connection establishment times in |load_timing_info| are
// null.  Its |connect_timing| field is ignored.
void ExpectLoadTimingHasOnlyConnectionTimes(
    const LoadTimingInfo& load_timing_info);

}  // namespace net

#endif  // NET_BASE_LOAD_TIMING_INFO_TEST_UTIL_H_

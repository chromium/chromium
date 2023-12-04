// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_timing_info_test_util.h"

#include "net/base/load_timing_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

void ExpectConnectTimingHasNoTimes(
    const LoadTimingInfo::ConnectTiming& connect_timing) {
  EXPECT_TRUE(connect_timing.domain_lookup_start.is_null());
  EXPECT_TRUE(connect_timing.domain_lookup_end.is_null());
  EXPECT_TRUE(connect_timing.connect_start.is_null());
  EXPECT_TRUE(connect_timing.connect_end.is_null());
  EXPECT_TRUE(connect_timing.ssl_start.is_null());
  EXPECT_TRUE(connect_timing.ssl_end.is_null());
}

void ExpectConnectTimingHasTimes(
    const LoadTimingInfo::ConnectTiming& connect_timing,
    int connect_timing_flags) {
  EXPECT_FALSE(connect_timing.connect_start.is_null());
  EXPECT_LE(connect_timing.connect_start, connect_timing.connect_end);

  if (!(connect_timing_flags & CONNECT_TIMING_HAS_DNS_TIMES)) {
    EXPECT_TRUE(connect_timing.domain_lookup_start.is_null());
    EXPECT_TRUE(connect_timing.domain_lookup_end.is_null());
  } else {
    EXPECT_FALSE(connect_timing.domain_lookup_start.is_null());
    EXPECT_LE(connect_timing.domain_lookup_start,
              connect_timing.domain_lookup_end);
    EXPECT_LE(connect_timing.domain_lookup_end, connect_timing.connect_start);
  }

  if (!(connect_timing_flags & CONNECT_TIMING_HAS_SSL_TIMES)) {
    EXPECT_TRUE(connect_timing.ssl_start.is_null());
    EXPECT_TRUE(connect_timing.ssl_end.is_null());
  } else {
    EXPECT_FALSE(connect_timing.ssl_start.is_null());
    EXPECT_LE(connect_timing.connect_start, connect_timing.ssl_start);
    EXPECT_LE(connect_timing.ssl_start, connect_timing.ssl_end);
    EXPECT_LE(connect_timing.ssl_end, connect_timing.connect_end);
  }
}

void ExpectLoadTimingHasOnlyConnectionTimes(
    const LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
  EXPECT_TRUE(load_timing_info.send_start.is_null());
  EXPECT_TRUE(load_timing_info.send_end.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
  EXPECT_TRUE(load_timing_info.first_early_hints_time.is_null());
  EXPECT_TRUE(load_timing_info.push_start.is_null());
  EXPECT_TRUE(load_timing_info.push_end.is_null());
}

}  // namespace net

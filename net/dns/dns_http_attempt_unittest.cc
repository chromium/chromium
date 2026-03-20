// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_http_attempt.h"

#include "base/values.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_partition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(DnsHTTPAttemptTest, DohIsolationInfo) {
  // Our requirements for the IsolationInfo are that the NAK is serializable
  // (so AltSvc information can be stored across restarts) and that it be
  // isolated from web requests.
  const IsolationInfo& isolation_info = DnsHTTPAttempt::GetDohIsolationInfo();
  base::Value out;
  EXPECT_TRUE(isolation_info.network_anonymization_key().ToValue(&out));
  EXPECT_EQ(isolation_info.GetNetworkIsolationPartition(),
            NetworkIsolationPartition::kDnsOverHttps);
  EXPECT_TRUE(NetworkIsolationPartitionAlwaysAllowEmptyPartition(
      NetworkIsolationPartition::kDnsOverHttps));
}

}  // namespace net

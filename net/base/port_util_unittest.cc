// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/port_util.h"

#include <array>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  const auto valid = std::to_array<std::vector<uint16_t>>({
      {},
      {1},
      {1, 2},
      {1, 2, 3},
      {10, 11, 12, 13},
  });

  for (size_t i = 0; i < std::size(valid); ++i) {
    SetExplicitlyAllowedPorts(valid[i]);
    EXPECT_EQ(i, GetCountOfExplicitlyAllowedPorts());
  }
}

TEST(NetUtilTest, RestrictedAbusePortsTest) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRestrictAbusePorts,
      {{"restrict_ports", "12345,23456,34567"}, {"monitor_ports", "45678"}});
  EXPECT_TRUE(IsPortAllowedForScheme(443, "https"));
  for (int port : {12345, 23456, 34567}) {
    EXPECT_FALSE(IsPortAllowedForScheme(port, "https"));
  }
  EXPECT_TRUE(IsPortAllowedForScheme(45678, "https"));
  histogram_tester.ExpectTotalCount("Net.RestrictedPorts", 4);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 12345, 1);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 23456, 1);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 34567, 1);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 45678, 1);
}

TEST(NetUtilTest, RestrictedAbusePortsLocalhostTest) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRestrictAbusePortsOnLocalhost,
      {{"localhost_restrict_ports", "12345,23456,34567"}});
  ReloadLocalhostRestrictedPortsForTesting();
  IPAddress public_address(8, 8, 8, 8);
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(
      IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(public_address), 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv4Localhost(), 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv6Localhost(), 443)));
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts", 0);

  const IPAddress localhost_addresses[] = {
      IPAddress::IPv4Localhost(),
      IPAddress(127, 0, 0, 2),
      IPAddress::IPv4AllZeros(),
      IPAddress::IPv6Localhost(),
      IPAddress::IPv6AllZeros(),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4Localhost()),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4AllZeros()),
  };

  int expected_count = 0;
  for (int port : {12345, 23456, 34567}) {
    for (const auto& address : localhost_addresses) {
      EXPECT_FALSE(IsPortAllowedForIpEndpoint(IPEndPoint(address, port)));
      expected_count++;
    }
  }
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts",
                                    expected_count);
  histogram_tester.ExpectBucketCount("Net.RestrictedLocalhostPorts", 12345,
                                     std::size(localhost_addresses));
  histogram_tester.ExpectBucketCount("Net.RestrictedLocalhostPorts", 23456,
                                     std::size(localhost_addresses));
  histogram_tester.ExpectBucketCount("Net.RestrictedLocalhostPorts", 34567,
                                     std::size(localhost_addresses));
}

TEST(NetUtilTest, RestrictedAbusePortsLocalhostTestNoParamSet) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kRestrictAbusePortsOnLocalhost);
  ReloadLocalhostRestrictedPortsForTesting();
  IPAddress public_address(8, 8, 8, 8);
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(
      IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(public_address), 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv4Localhost(), 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv6Localhost(), 443)));
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts", 0);

  const IPAddress localhost_addresses[] = {
      IPAddress::IPv4Localhost(),
      IPAddress(127, 0, 0, 2),
      IPAddress::IPv4AllZeros(),
      IPAddress::IPv6Localhost(),
      IPAddress::IPv6AllZeros(),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4Localhost()),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4AllZeros()),
  };

  for (int port : {12345, 23456, 34567}) {
    for (const auto& address : localhost_addresses) {
      EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(address, port)));
    }
  }
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts", 0);
}

}  // namespace net

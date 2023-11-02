// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_address_mismatch.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

// Test all cases of the GetAddressMismatch function.
TEST(QuicAddressMismatchTest, GetAddressMismatch) {
  IPAddress ip4_1;
  IPAddress ip4_2;
  IPAddress ip6_1;
  IPAddress ip6_2;
  IPAddress ip4_mapped_1;
  IPAddress ip4_mapped_2;
  ASSERT_TRUE(ip4_1.AssignFromIPLiteral("1.2.3.4"));
  ASSERT_TRUE(ip4_2.AssignFromIPLiteral("5.6.7.8"));
  ASSERT_TRUE(ip6_1.AssignFromIPLiteral("1234::1"));
  ASSERT_TRUE(ip6_2.AssignFromIPLiteral("1234::2"));
  ip4_mapped_1 = ConvertIPv4ToIPv4MappedIPv6(ip4_1);
  ip4_mapped_2 = ConvertIPv4ToIPv4MappedIPv6(ip4_2);
  ASSERT_NE(ip4_1, ip4_2);
  ASSERT_NE(ip6_1, ip6_2);
  ASSERT_NE(ip4_mapped_1, ip4_mapped_2);

  EXPECT_EQ(-1, GetAddressMismatch(IPEndPoint(), IPEndPoint()));
  EXPECT_EQ(-1, GetAddressMismatch(IPEndPoint(), IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(-1, GetAddressMismatch(IPEndPoint(ip4_1, 443), IPEndPoint()));

  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443), IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 443),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 443), IPEndPoint(ip6_1, 443)));

  EXPECT_EQ(QUIC_PORT_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 80), IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(
      QUIC_PORT_MISMATCH_V4_V4,
      GetAddressMismatch(IPEndPoint(ip4_1, 80), IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_PORT_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 80),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_PORT_MISMATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 80), IPEndPoint(ip6_1, 443)));

  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443), IPEndPoint(ip4_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443),
                               IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 443),
                               IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 80), IPEndPoint(ip4_2, 443)));
  EXPECT_EQ(
      QUIC_ADDRESS_MISMATCH_V4_V4,
      GetAddressMismatch(IPEndPoint(ip4_1, 80), IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 80),
                               IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 443), IPEndPoint(ip6_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 80), IPEndPoint(ip6_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V6,
            GetAddressMismatch(IPEndPoint(ip4_1, 443), IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V6,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 443),
                               IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V6,
            GetAddressMismatch(IPEndPoint(ip4_1, 80), IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(
      QUIC_ADDRESS_MISMATCH_V4_V6,
      GetAddressMismatch(IPEndPoint(ip4_mapped_1, 80), IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V4,
            GetAddressMismatch(IPEndPoint(ip6_1, 443), IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V4,
            GetAddressMismatch(IPEndPoint(ip6_1, 443),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V4,
            GetAddressMismatch(IPEndPoint(ip6_1, 80), IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(
      QUIC_ADDRESS_MISMATCH_V6_V4,
      GetAddressMismatch(IPEndPoint(ip6_1, 80), IPEndPoint(ip4_mapped_1, 443)));
}

}  // namespace net::test

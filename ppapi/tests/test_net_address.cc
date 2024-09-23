// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_net_address.h"

#include <stddef.h>
#include <stdint.h>

#include <cstring>

#include "ppapi/cpp/net_address.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

using pp::NetAddress;

REGISTER_TEST_CASE(NetAddress);

namespace {

bool EqualIPv4Address(const PP_NetAddress_IPv4& addr1,
                      const PP_NetAddress_IPv4& addr2) {
  return addr1.port == addr2.port &&
         !memcmp(addr1.addr, addr2.addr, sizeof(addr1.addr));
}

bool EqualIPv6Address(const PP_NetAddress_IPv6& addr1,
                      const PP_NetAddress_IPv6& addr2) {
  return addr1.port == addr2.port &&
         !memcmp(addr1.addr, addr2.addr, sizeof(addr1.addr));
}

NetAddress CreateFromHostOrderIPv6Address(
    const pp::InstanceHandle& instance,
    const uint16_t host_order_addr[8],
    uint16_t host_order_port) {
  PP_NetAddress_IPv6 ipv6_addr;
  ipv6_addr.port = ConvertToNetEndian16(host_order_port);
  for (size_t i = 0; i < 8; ++i) {
    uint16_t net_order_addr = ConvertToNetEndian16(host_order_addr[i]);
    memcpy(&ipv6_addr.addr[2 * i], &net_order_addr, 2);
  }

  return NetAddress(instance, ipv6_addr);
}

}  // namespace

TestNetAddress::TestNetAddress(TestingInstance* instance) : TestCase(instance) {
}

bool TestNetAddress::Init() {
  return NetAddress::IsAvailable();
}

void TestNetAddress::RunTests(const std::string& filter) {
  RUN_TEST(IPv4Address, filter);
  RUN_TEST(IPv6Address, filter);
  RUN_TEST(DescribeAsString, filter);
}

std::string TestNetAddress::TestIPv4Address() {
  PP_NetAddress_IPv4 ipv4_addr = { ConvertToNetEndian16(80), { 127, 0, 0, 1 } };
  NetAddress net_addr(instance_, ipv4_addr);
  ASSERT_NE(0, net_addr.pp_resource());

  ASSERT_EQ(PP_NETADDRESS_FAMILY_IPV4, net_addr.GetFamily());

  PP_NetAddress_IPv4 out_ipv4_addr;
  ASSERT_TRUE(net_addr.DescribeAsIPv4Address(&out_ipv4_addr));
  ASSERT_TRUE(EqualIPv4Address(ipv4_addr, out_ipv4_addr));

  PP_NetAddress_IPv6 out_ipv6_addr;
  ASSERT_FALSE(net_addr.DescribeAsIPv6Address(&out_ipv6_addr));

  PASS();
}

std::string TestNetAddress::TestIPv6Address() {
  PP_NetAddress_IPv6 ipv6_addr = {
    ConvertToNetEndian16(1024),
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
  };

  NetAddress net_addr(instance_, ipv6_addr);
  ASSERT_NE(0, net_addr.pp_resource());

  ASSERT_EQ(PP_NETADDRESS_FAMILY_IPV6, net_addr.GetFamily());

  PP_NetAddress_IPv6 out_ipv6_addr;
  ASSERT_TRUE(net_addr.DescribeAsIPv6Address(&out_ipv6_addr));
  ASSERT_TRUE(EqualIPv6Address(ipv6_addr, out_ipv6_addr));

  PP_NetAddress_IPv4 out_ipv4_addr;
  ASSERT_FALSE(net_addr.DescribeAsIPv4Address(&out_ipv4_addr));

  PASS();
}

std::string TestNetAddress::TestDescribeAsString() {
  {
    // Test describing IPv4 addresses.
    PP_NetAddress_IPv4 ipv4_addr1 = { ConvertToNetEndian16(1234),
                                      { 127, 0, 0, 1 } };
    NetAddress addr1(instance_, ipv4_addr1);
    ASSERT_EQ("127.0.0.1", addr1.DescribeAsString(false).AsString());
    ASSERT_EQ("127.0.0.1:1234", addr1.DescribeAsString(true).AsString());

    PP_NetAddress_IPv4 ipv4_addr2 = { ConvertToNetEndian16(80),
                                      { 192, 168, 0, 2 } };
    NetAddress addr2(instance_, ipv4_addr2);
    ASSERT_EQ("192.168.0.2", addr2.DescribeAsString(false).AsString());
    ASSERT_EQ("192.168.0.2:80", addr2.DescribeAsString(true).AsString());
  }
  {
    // Test describing IPv6 addresses.
    static const struct {
      uint16_t host_order_addr[8];
      uint16_t host_order_port;
      const char* expected_without_port;
      const char* expected_with_port;
    } ipv6_test_cases[] = {
      {  // Generic test case (unique longest run of zeros to collapse).
        { 0x12, 0xabcd, 0, 0x0001, 0, 0, 0, 0xcdef }, 12,
        "12:abcd:0:1::cdef", "[12:abcd:0:1::cdef]:12"
      },
      {  // Ignore the first (non-longest) run of zeros.
        { 0, 0, 0, 0x0123, 0, 0, 0, 0 }, 123,
        "0:0:0:123::", "[0:0:0:123::]:123"
      },
      {  // Collapse the first (equally-longest) run of zeros.
        { 0x1234, 0xabcd, 0, 0, 0xff, 0, 0, 0xcdef }, 123,
        "1234:abcd::ff:0:0:cdef", "[1234:abcd::ff:0:0:cdef]:123"
      },
      {  // Don't collapse "runs" of zeros of length 1.
        { 0, 0xa, 1, 2, 3, 0, 5, 0 }, 123,
        "0:a:1:2:3:0:5:0", "[0:a:1:2:3:0:5:0]:123"
      },
      {  // Collapse a run of zeros at the beginning.
        { 0, 0, 0, 2, 3, 0, 0, 0 }, 123,
        "::2:3:0:0:0", "[::2:3:0:0:0]:123"
      },
      {  // Collapse a run of zeros at the end.
        { 0, 0xa, 1, 2, 3, 0, 0, 0 }, 123,
        "0:a:1:2:3::", "[0:a:1:2:3::]:123"
      },
      {  // IPv4 192.168.1.2 embedded in IPv6 in the deprecated way.
        { 0, 0, 0, 0, 0, 0, 0xc0a8, 0x102 }, 123,
        "::192.168.1.2", "[::192.168.1.2]:123"
      },
      {  // IPv4 192.168.1.2 embedded in IPv6.
        { 0, 0, 0, 0, 0, 0xffff, 0xc0a8, 0x102 }, 123,
        "::ffff:192.168.1.2", "[::ffff:192.168.1.2]:123"
      },
      {  // *Not* IPv4 embedded in IPv6.
        { 0, 0, 0, 0, 0, 0x1234, 0xc0a8, 0x102 }, 123,
        "::1234:c0a8:102", "[::1234:c0a8:102]:123"
      }
    };

    for (size_t i = 0;
         i < sizeof(ipv6_test_cases) / sizeof(ipv6_test_cases[0]);
         ++i) {
      NetAddress addr = CreateFromHostOrderIPv6Address(
          instance_, ipv6_test_cases[i].host_order_addr,
          ipv6_test_cases[i].host_order_port);
      ASSERT_EQ(ipv6_test_cases[i].expected_without_port,
                addr.DescribeAsString(false).AsString());
      ASSERT_EQ(ipv6_test_cases[i].expected_with_port,
                addr.DescribeAsString(true).AsString());
    }
  }

  PASS();
}

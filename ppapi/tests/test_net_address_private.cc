// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_net_address_private.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

using pp::NetAddressPrivate;

namespace {

PP_NetAddress_Private MakeIPv4NetAddress(const uint8_t host[4], int port) {
  PP_NetAddress_Private addr;
  NetAddressPrivate::CreateFromIPv4Address(host, port, &addr);
  return addr;
}

PP_NetAddress_Private MakeIPv6NetAddress(const uint16_t host[8], uint16_t port,
                                         uint32_t scope_id) {
  PP_NetAddress_Private addr = PP_NetAddress_Private();
  uint8_t ip[16];
  for(int i = 0; i < 8; ++i) {
    ip[i * 2] = host[i] >> 8;
    ip[i * 2 + 1] = host[i] & 0xff;
  }
  NetAddressPrivate::CreateFromIPv6Address(ip, scope_id, port, &addr);
  return addr;
}

}  // namespace

REGISTER_TEST_CASE(NetAddressPrivate);

TestNetAddressPrivate::TestNetAddressPrivate(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestNetAddressPrivate::Init() {
  return NetAddressPrivate::IsAvailable();
}

void TestNetAddressPrivate::RunTests(const std::string& filter) {
  RUN_TEST(AreEqual, filter);
  RUN_TEST(AreHostsEqual, filter);
  RUN_TEST(Describe, filter);
  RUN_TEST(ReplacePort, filter);
  RUN_TEST(GetAnyAddress, filter);
  RUN_TEST(DescribeIPv6, filter);
  RUN_TEST(GetFamily, filter);
  RUN_TEST(GetPort, filter);
  RUN_TEST(GetAddress, filter);
  RUN_TEST(GetScopeID, filter);
}

std::string TestNetAddressPrivate::TestAreEqual() {
  // No comparisons should ever be done with invalid addresses.
  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_FALSE(NetAddressPrivate::AreEqual(invalid, invalid));

  uint8_t localhost_ip[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress(localhost_ip, 80);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(localhost_80, localhost_80));
  ASSERT_FALSE(NetAddressPrivate::AreEqual(localhost_80, invalid));

  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress(localhost_ip, 1234);
  ASSERT_FALSE(NetAddressPrivate::AreEqual(localhost_80, localhost_1234));

  uint8_t other_ip[4] = { 192, 168, 0, 1 };
  PP_NetAddress_Private other_80 = MakeIPv4NetAddress(other_ip, 80);
  ASSERT_FALSE(NetAddressPrivate::AreEqual(localhost_80, other_80));

  PASS();
}

std::string TestNetAddressPrivate::TestAreHostsEqual() {
  // No comparisons should ever be done with invalid addresses.
  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_FALSE(NetAddressPrivate::AreHostsEqual(invalid, invalid));

  uint8_t localhost_ip[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress(localhost_ip, 80);
  ASSERT_TRUE(NetAddressPrivate::AreHostsEqual(localhost_80, localhost_80));
  ASSERT_FALSE(NetAddressPrivate::AreHostsEqual(localhost_80, invalid));

  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress(localhost_ip, 1234);
  ASSERT_TRUE(NetAddressPrivate::AreHostsEqual(localhost_80, localhost_1234));

  uint8_t other_ip[4] = { 192, 168, 0, 1 };
  PP_NetAddress_Private other_80 = MakeIPv4NetAddress(other_ip, 80);
  ASSERT_FALSE(NetAddressPrivate::AreHostsEqual(localhost_80, other_80));

  PASS();
}

std::string TestNetAddressPrivate::TestDescribe() {
  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_EQ("", NetAddressPrivate::Describe(invalid, false));
  ASSERT_EQ("", NetAddressPrivate::Describe(invalid, true));

  uint8_t localhost_ip[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress(localhost_ip, 80);
  ASSERT_EQ("127.0.0.1", NetAddressPrivate::Describe(localhost_80, false));
  ASSERT_EQ("127.0.0.1:80", NetAddressPrivate::Describe(localhost_80, true));

  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress(localhost_ip, 1234);
  ASSERT_EQ("127.0.0.1", NetAddressPrivate::Describe(localhost_1234, false));
  ASSERT_EQ("127.0.0.1:1234", NetAddressPrivate::Describe(localhost_1234,
                                                          true));

  uint8_t other_ip[4] = { 192, 168, 0, 1 };
  PP_NetAddress_Private other_80 = MakeIPv4NetAddress(other_ip, 80);
  ASSERT_EQ("192.168.0.1", NetAddressPrivate::Describe(other_80, false));
  ASSERT_EQ("192.168.0.1:80", NetAddressPrivate::Describe(other_80, true));

  PASS();
}

std::string TestNetAddressPrivate::TestReplacePort() {
  // Assume that |AreEqual()| works correctly.
  PP_NetAddress_Private result = PP_NetAddress_Private();

  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_FALSE(NetAddressPrivate::ReplacePort(invalid, 1234, &result));

  uint8_t localhost_ip[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress(localhost_ip, 80);
  ASSERT_TRUE(NetAddressPrivate::ReplacePort(localhost_80, 1234, &result));
  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress(localhost_ip, 1234);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, localhost_1234));

  // Test that having the out param being the same as the in param works
  // properly.
  ASSERT_TRUE(NetAddressPrivate::ReplacePort(result, 80, &result));
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, localhost_80));

  PASS();
}

std::string TestNetAddressPrivate::TestGetAnyAddress() {
  // Just make sure it doesn't crash and such.
  PP_NetAddress_Private result = PP_NetAddress_Private();

  NetAddressPrivate::GetAnyAddress(false, &result);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, result));

  NetAddressPrivate::GetAnyAddress(true, &result);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, result));

  PASS();
}

// TODO(viettrungluu): More IPv6 tests needed.

std::string TestNetAddressPrivate::TestDescribeIPv6() {
  static const struct {
    uint16_t address[8];
    uint16_t port;
    uint32_t scope;
    const char* expected_without_port;
    const char* expected_with_port;
  } test_cases[] = {
    {  // Generic test case (unique longest run of zeros to collapse).
      { 0x12, 0xabcd, 0, 0x0001, 0, 0, 0, 0xcdef }, 12, 0,
      "12:abcd:0:1::cdef", "[12:abcd:0:1::cdef]:12"
    },
    {  // Non-zero scope.
      { 0x1234, 0xabcd, 0, 0x0001, 0, 0, 0, 0xcdef }, 1234, 789,
      "1234:abcd:0:1::cdef%789", "[1234:abcd:0:1::cdef%789]:1234"
    },
    {  // Ignore the first (non-longest) run of zeros.
      { 0, 0, 0, 0x0123, 0, 0, 0, 0 }, 123, 0,
      "0:0:0:123::", "[0:0:0:123::]:123"
    },
    {  // Collapse the first (equally-longest) run of zeros.
      { 0x1234, 0xabcd, 0, 0, 0xff, 0, 0, 0xcdef }, 123, 0,
      "1234:abcd::ff:0:0:cdef", "[1234:abcd::ff:0:0:cdef]:123"
    },
    {  // Don't collapse "runs" of zeros of length 1.
      { 0, 0xa, 1, 2, 3, 0, 5, 0 }, 123, 0,
      "0:a:1:2:3:0:5:0", "[0:a:1:2:3:0:5:0]:123"
    },
    {  // Collapse a run of zeros at the beginning.
      { 0, 0, 0, 2, 3, 0, 0, 0 }, 123, 0,
      "::2:3:0:0:0", "[::2:3:0:0:0]:123"
    },
    {  // Collapse a run of zeros at the end.
      { 0, 0xa, 1, 2, 3, 0, 0, 0 }, 123, 0,
      "0:a:1:2:3::", "[0:a:1:2:3::]:123"
    },
    {  // IPv4 192.168.1.2 embedded in IPv6 in the deprecated way.
      { 0, 0, 0, 0, 0, 0, 0xc0a8, 0x102 }, 123, 0,
      "::192.168.1.2", "[::192.168.1.2]:123"
    },
    {  // ... with non-zero scope.
      { 0, 0, 0, 0, 0, 0, 0xc0a8, 0x102 }, 123, 789,
      "::192.168.1.2%789", "[::192.168.1.2%789]:123"
    },
    {  // IPv4 192.168.1.2 embedded in IPv6.
      { 0, 0, 0, 0, 0, 0xffff, 0xc0a8, 0x102 }, 123, 0,
      "::ffff:192.168.1.2", "[::ffff:192.168.1.2]:123"
    },
    {  // ... with non-zero scope.
      { 0, 0, 0, 0, 0, 0xffff, 0xc0a8, 0x102 }, 123, 789,
      "::ffff:192.168.1.2%789", "[::ffff:192.168.1.2%789]:123"
    },
    {  // *Not* IPv4 embedded in IPv6.
      { 0, 0, 0, 0, 0, 0x1234, 0xc0a8, 0x102 }, 123, 0,
      "::1234:c0a8:102", "[::1234:c0a8:102]:123"
    }
  };

  for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
    PP_NetAddress_Private addr = MakeIPv6NetAddress(test_cases[i].address,
                                                    test_cases[i].port,
                                                    test_cases[i].scope);
    ASSERT_EQ(test_cases[i].expected_without_port,
              NetAddressPrivate::Describe(addr, false));
    ASSERT_EQ(test_cases[i].expected_with_port,
              NetAddressPrivate::Describe(addr, true));
  }

  PASS();
}

std::string TestNetAddressPrivate::TestGetFamily() {
  uint8_t localhost_ip[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private ipv4 = MakeIPv4NetAddress(localhost_ip, 80);
  ASSERT_EQ(NetAddressPrivate::GetFamily(ipv4),
            PP_NETADDRESSFAMILY_PRIVATE_IPV4);

  uint16_t ipv6_address[8] = { 0x1234, 0xabcd, 0, 0, 0xff, 0, 0, 0xcdef };
  PP_NetAddress_Private ipv6 = MakeIPv6NetAddress(ipv6_address, 123, 0);
  ASSERT_EQ(NetAddressPrivate::GetFamily(ipv6),
            PP_NETADDRESSFAMILY_PRIVATE_IPV6);

  PASS();
}

std::string TestNetAddressPrivate::TestGetPort() {
  uint8_t localhost_ip[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress(localhost_ip, 80);
  ASSERT_EQ(NetAddressPrivate::GetPort(localhost_80), 80);

  uint16_t ipv6_address[8] = { 0x1234, 0xabcd, 0, 0, 0xff, 0, 0, 0xcdef };

  PP_NetAddress_Private port_123 = MakeIPv6NetAddress(ipv6_address, 123, 0);
  ASSERT_EQ(NetAddressPrivate::GetPort(port_123), 123);

  PP_NetAddress_Private port_FFFF = MakeIPv6NetAddress(ipv6_address,
                                                       0xFFFF,
                                                       0);
  ASSERT_EQ(NetAddressPrivate::GetPort(port_FFFF), 0xFFFF);

  PASS();
}

std::string TestNetAddressPrivate::TestGetAddress() {
  const int addr_storage_len = 16;
  unsigned char addr_storage[addr_storage_len];

  const uint8_t ipv4_addr[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress(ipv4_addr, 80);
  memset(addr_storage, 0, addr_storage_len);
  ASSERT_TRUE(NetAddressPrivate::GetAddress(localhost_80,
                                            addr_storage,
                                            addr_storage_len));
  ASSERT_EQ(memcmp(addr_storage, &ipv4_addr, 4), 0);

  // Insufficient storage for address.
  ASSERT_FALSE(NetAddressPrivate::GetAddress(localhost_80,
                                             addr_storage,
                                             1));

  uint16_t ipv6_address[8] = { 0x1234, 0xabcd, 0, 0, 0xff, 0, 0, 0xcdef };
  PP_NetAddress_Private ipv6_addr = MakeIPv6NetAddress(ipv6_address,
                                                       123,
                                                       0);

  // Ensure the ipv6 address is transformed properly into network order.
  uint8_t ipv6_bytes[16];
  for(int i = 0; i < 8; ++i) {
    ipv6_bytes[i * 2] = ipv6_address[i] >> 8;
    ipv6_bytes[i * 2 + 1] = ipv6_address[i] & 0xFF;
  }

  memset(addr_storage, 0, addr_storage_len);
  ASSERT_TRUE(NetAddressPrivate::GetAddress(ipv6_addr,
                                            addr_storage,
                                            addr_storage_len));
  ASSERT_EQ(memcmp(addr_storage, ipv6_bytes, 16), 0);

  // Insufficient storage for address.
  ASSERT_FALSE(NetAddressPrivate::GetAddress(ipv6_addr,
                                             addr_storage,
                                             1));

  PASS();
}

std::string TestNetAddressPrivate::TestGetScopeID() {
  uint8_t localhost_ip[4] = { 127, 0, 0, 1 };
  PP_NetAddress_Private ipv4 = MakeIPv4NetAddress(localhost_ip, 80);
  ASSERT_EQ(0, NetAddressPrivate::GetScopeID(ipv4));

  uint16_t ipv6_address[8] = { 0x1234, 0xabcd, 0, 0, 0xff, 0, 0, 0xcdef };

  PP_NetAddress_Private ipv6_123 = MakeIPv6NetAddress(ipv6_address, 0, 123);
  ASSERT_EQ(123, NetAddressPrivate::GetScopeID(ipv6_123));

  PP_NetAddress_Private ipv6_max =
      MakeIPv6NetAddress(ipv6_address, 0, 0xFFFFFFFF);
  ASSERT_EQ(NetAddressPrivate::GetScopeID(ipv6_max), 0xFFFFFFFF);

  PASS();
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"

#include <string.h>

#include <optional>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/sys_addrinfo.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>

#include <ws2bth.h>

#include "base/test/gtest_util.h"   // For EXPECT_DCHECK_DEATH
#include "net/base/winsock_util.h"  // For kBluetoothAddressSize
#elif BUILDFLAG(IS_POSIX)
#include <netinet/in.h>
#endif

using testing::Optional;

namespace net {

namespace {

// Retuns the port field of the |sockaddr|.
const uint16_t* GetPortFieldFromSockaddr(const struct sockaddr* address,
                                         socklen_t address_len) {
  if (address->sa_family == AF_INET) {
    DCHECK_LE(sizeof(sockaddr_in), static_cast<size_t>(address_len));
    const struct sockaddr_in* sockaddr =
        reinterpret_cast<const struct sockaddr_in*>(address);
    return &sockaddr->sin_port;
  } else if (address->sa_family == AF_INET6) {
    DCHECK_LE(sizeof(sockaddr_in6), static_cast<size_t>(address_len));
    const struct sockaddr_in6* sockaddr =
        reinterpret_cast<const struct sockaddr_in6*>(address);
    return &sockaddr->sin6_port;
  } else {
    NOTREACHED();
  }
}

// Returns the value of port in |sockaddr| (in host byte ordering).
int GetPortFromSockaddr(const struct sockaddr* address, socklen_t address_len) {
  const uint16_t* port_field = GetPortFieldFromSockaddr(address, address_len);
  if (!port_field)
    return -1;
  return base::NetToHost16(*port_field);
}

constexpr uint32_t kMaxFakeInterfaceIndex = 10;

uint32_t FakeNameToIndexFunc(const char* name) {
  uint32_t index = 0;
  const bool ok = base::StringToUint(name, &index);
  if (!ok || index > kMaxFakeInterfaceIndex) {
    return 0;
  }
  return index;
}

char* FakeIndexToNameFunc(unsigned int index, base::span<char>ifname) {
  if (index > kMaxFakeInterfaceIndex) {
    return nullptr;
  }
  std::string name = base::NumberToString(index);
  ifname[0] = name[0];
  return ifname.data();
}

struct TestData {
  std::string host;
  std::string host_normalized;
  bool ipv6;
  IPAddress ip_address;
  std::optional<uint32_t> scope_id = std::nullopt;
} tests[] = {
    {"127.0.00.1", "127.0.0.1", false},
    {"192.168.1.1", "192.168.1.1", false},
    {"::1", "[::1]", true},
    {"2001:db8:0::42", "[2001:db8::42]", true},
    {"fe80::1", "[fe80::1]", true, IPAddress(), /*scope_id=*/1},
};

class IPEndPointTest : public PlatformTest {
 public:
  void SetUp() override {
    IPEndPoint::SetNameToIndexFuncForTesting(FakeNameToIndexFunc);
    IPEndPoint::SetIndexToNameFuncForTesting(FakeIndexToNameFunc);

    // This is where we populate the TestData.
    for (auto& test : tests) {
      EXPECT_TRUE(test.ip_address.AssignFromIPLiteral(test.host));
    }
  }

  void TearDown() override {
    IPEndPoint::SetNameToIndexFuncForTesting(nullptr);
    IPEndPoint::SetIndexToNameFuncForTesting(nullptr);
  }
};

TEST_F(IPEndPointTest, Constructor) {
  {
    IPEndPoint endpoint;
    EXPECT_EQ(0, endpoint.port());
  }

  for (const auto& test : tests) {
    IPEndPoint endpoint(test.ip_address, 80, test.scope_id);
    EXPECT_EQ(80, endpoint.port());
    EXPECT_EQ(test.ip_address, endpoint.address());
    EXPECT_EQ(test.scope_id, endpoint.scope_id());
  }
}

TEST_F(IPEndPointTest, Assignment) {
  uint16_t port = 0;
  for (const auto& test : tests) {
    IPEndPoint src(test.ip_address, ++port, test.scope_id);
    IPEndPoint dest = src;

    EXPECT_EQ(src.port(), dest.port());
    EXPECT_EQ(src.address(), dest.address());
    EXPECT_EQ(src.scope_id(), dest.scope_id());
  }
}

TEST_F(IPEndPointTest, Copy) {
  uint16_t port = 0;
  for (const auto& test : tests) {
    IPEndPoint src(test.ip_address, ++port, test.scope_id);
    IPEndPoint dest(src);

    EXPECT_EQ(src.port(), dest.port());
    EXPECT_EQ(src.address(), dest.address());
    EXPECT_EQ(src.scope_id(), dest.scope_id());
  }
}

TEST_F(IPEndPointTest, ToFromSockAddr) {
  uint16_t port = 0;
  for (const auto& test : tests) {
    IPEndPoint ip_endpoint(test.ip_address, ++port, test.scope_id);

    // Convert to a sockaddr.
    SockaddrStorage storage;
    EXPECT_TRUE(ip_endpoint.ToSockAddr(storage.addr(), &storage.addr_len));

    // Basic verification.
    socklen_t expected_size =
        test.ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    EXPECT_EQ(expected_size, storage.addr_len);
    EXPECT_EQ(ip_endpoint.port(),
              GetPortFromSockaddr(storage.addr(), storage.addr_len));
    if (test.ipv6) {
      uint32_t scope_id =
          reinterpret_cast<struct sockaddr_in6*>(storage.addr())->sin6_scope_id;
      EXPECT_EQ(scope_id, test.scope_id.value_or(0));
    }
    // And convert back to an IPEndPoint.
    IPEndPoint ip_endpoint2;
    EXPECT_TRUE(ip_endpoint2.FromSockAddr(storage.addr(), storage.addr_len));
    EXPECT_EQ(ip_endpoint.port(), ip_endpoint2.port());
    EXPECT_EQ(ip_endpoint.address(), ip_endpoint2.address());
    EXPECT_EQ(ip_endpoint.scope_id(), ip_endpoint2.scope_id());
  }
}

TEST_F(IPEndPointTest, ToSockAddrBufTooSmall) {
  uint16_t port = 0;
  for (const auto& test : tests) {
    IPEndPoint ip_endpoint(test.ip_address, port);

    SockaddrStorage storage;
    storage.addr_len = 3;  // size is too small!
    EXPECT_FALSE(ip_endpoint.ToSockAddr(storage.addr(), &storage.addr_len));
  }
}

TEST_F(IPEndPointTest, FromSockAddrBufTooSmall) {
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  IPEndPoint ip_endpoint;
  struct sockaddr* sockaddr = reinterpret_cast<struct sockaddr*>(&addr);
  EXPECT_FALSE(ip_endpoint.FromSockAddr(sockaddr, sizeof(addr) - 1));
}

#if BUILDFLAG(IS_WIN)

namespace {
constexpr uint8_t kBluetoothAddrBytes[kBluetoothAddressSize] = {1, 2, 3,
                                                                4, 5, 6};
constexpr uint8_t kBluetoothAddrBytes2[kBluetoothAddressSize] = {1, 2, 3,
                                                                 4, 5, 7};
const IPAddress kBluetoothAddress(kBluetoothAddrBytes);
const IPAddress kBluetoothAddress2(kBluetoothAddrBytes2);

// Select a Bluetooth port that does not fit in a uint16_t.
constexpr uint32_t kBluetoothPort = std::numeric_limits<uint16_t>::max() + 1;

SOCKADDR_BTH BuildBluetoothSockAddr(const IPAddress& ip_address,
                                    uint32_t port) {
  SOCKADDR_BTH addr = {};
  addr.addressFamily = AF_BTH;
  base::byte_span_from_ref(addr.btAddr).copy_prefix_from(ip_address.bytes());
  addr.port = port;
  return addr;
}
}  // namespace

TEST_F(IPEndPointTest, WinBluetoothSockAddrCompareWithSelf) {
  IPEndPoint bt_endpoint;
  SOCKADDR_BTH addr = BuildBluetoothSockAddr(kBluetoothAddress, kBluetoothPort);
  EXPECT_TRUE(bt_endpoint.FromSockAddr(
      reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)));
  EXPECT_EQ(bt_endpoint.address(), kBluetoothAddress);
  EXPECT_EQ(bt_endpoint.GetFamily(), AddressFamily::ADDRESS_FAMILY_UNSPECIFIED);
  EXPECT_EQ(bt_endpoint.GetSockAddrFamily(), AF_BTH);
  // Comparison functions should agree that `bt_endpoint` equals itself.
  EXPECT_FALSE(bt_endpoint < bt_endpoint);
  EXPECT_FALSE(bt_endpoint != bt_endpoint);
  EXPECT_TRUE(bt_endpoint == bt_endpoint);
  // Test that IPv4/IPv6-only methods crash.
  EXPECT_DCHECK_DEATH(bt_endpoint.port());
  SockaddrStorage storage;
  EXPECT_DCHECK_DEATH(
      std::ignore = bt_endpoint.ToSockAddr(storage.addr(), &storage.addr_len));
  EXPECT_DCHECK_DEATH(bt_endpoint.ToString());
  EXPECT_DCHECK_DEATH(bt_endpoint.ToStringWithoutPort());
}

TEST_F(IPEndPointTest, WinBluetoothSockAddrCompareWithNonBluetooth) {
  IPEndPoint bt_endpoint;
  SOCKADDR_BTH addr = BuildBluetoothSockAddr(kBluetoothAddress, kBluetoothPort);
  EXPECT_TRUE(bt_endpoint.FromSockAddr(
      reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)));

  // Compare `bt_endpoint` with non-Bluetooth endpoints.
  for (const auto& test : tests) {
    IPEndPoint endpoint(test.ip_address, 80);
    if (test.ip_address.IsIPv4()) {
      EXPECT_FALSE(bt_endpoint < endpoint);
    } else {
      EXPECT_TRUE(test.ip_address.IsIPv6());
      EXPECT_TRUE(bt_endpoint < endpoint);
    }
    EXPECT_TRUE(bt_endpoint != endpoint);
    EXPECT_FALSE(bt_endpoint == endpoint);
  }
}

TEST_F(IPEndPointTest, WinBluetoothSockAddrCompareWithCopy) {
  IPEndPoint bt_endpoint;
  SOCKADDR_BTH addr = BuildBluetoothSockAddr(kBluetoothAddress, kBluetoothPort);
  EXPECT_TRUE(bt_endpoint.FromSockAddr(
      reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)));

  // Verify that a copy's accessors return the same values as the original's.
  IPEndPoint bt_endpoint_other(bt_endpoint);
  EXPECT_EQ(bt_endpoint.address(), bt_endpoint_other.address());
  EXPECT_EQ(bt_endpoint.GetFamily(), bt_endpoint_other.GetFamily());
  EXPECT_EQ(bt_endpoint.GetSockAddrFamily(),
            bt_endpoint_other.GetSockAddrFamily());
  // Comparison functions should agree that the endpoints are equal.
  EXPECT_FALSE(bt_endpoint < bt_endpoint_other);
  EXPECT_FALSE(bt_endpoint != bt_endpoint_other);
  EXPECT_TRUE(bt_endpoint == bt_endpoint_other);
  // Test that IPv4/IPv6-only methods crash.
  EXPECT_DCHECK_DEATH(bt_endpoint_other.port());
  SockaddrStorage storage;
  EXPECT_DCHECK_DEATH(std::ignore = bt_endpoint_other.ToSockAddr(
                          storage.addr(), &storage.addr_len));
  EXPECT_DCHECK_DEATH(bt_endpoint_other.ToString());
  EXPECT_DCHECK_DEATH(bt_endpoint_other.ToStringWithoutPort());
}

TEST_F(IPEndPointTest, WinBluetoothSockAddrCompareWithDifferentPort) {
  IPEndPoint bt_endpoint;
  SOCKADDR_BTH addr = BuildBluetoothSockAddr(kBluetoothAddress, kBluetoothPort);
  EXPECT_TRUE(bt_endpoint.FromSockAddr(
      reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)));

  // Compare with another IPEndPoint that has a different port.
  IPEndPoint bt_endpoint_other;
  SOCKADDR_BTH addr2 =
      BuildBluetoothSockAddr(kBluetoothAddress, kBluetoothPort + 1);
  EXPECT_TRUE(bt_endpoint_other.FromSockAddr(
      reinterpret_cast<const struct sockaddr*>(&addr2), sizeof(addr2)));
  EXPECT_EQ(bt_endpoint.address(), bt_endpoint_other.address());
  EXPECT_EQ(bt_endpoint.GetFamily(), bt_endpoint_other.GetFamily());
  EXPECT_EQ(bt_endpoint.GetSockAddrFamily(),
            bt_endpoint_other.GetSockAddrFamily());
  // Comparison functions should agree that `bt_endpoint == bt_endpoint_other`
  // because they have the same address and Bluetooth ports are not considered
  // by comparison functions.
  EXPECT_FALSE(bt_endpoint < bt_endpoint_other);
  EXPECT_FALSE(bt_endpoint != bt_endpoint_other);
  EXPECT_TRUE(bt_endpoint == bt_endpoint_other);
  // Test that IPv4/IPv6-only methods crash.
  EXPECT_DCHECK_DEATH(bt_endpoint_other.port());
  SockaddrStorage storage;
  EXPECT_DCHECK_DEATH(std::ignore = bt_endpoint_other.ToSockAddr(
                          storage.addr(), &storage.addr_len));
  EXPECT_DCHECK_DEATH(bt_endpoint_other.ToString());
  EXPECT_DCHECK_DEATH(bt_endpoint_other.ToStringWithoutPort());
}

TEST_F(IPEndPointTest, WinBluetoothSockAddrCompareWithDifferentAddress) {
  IPEndPoint bt_endpoint;
  SOCKADDR_BTH addr = BuildBluetoothSockAddr(kBluetoothAddress, kBluetoothPort);
  EXPECT_TRUE(bt_endpoint.FromSockAddr(
      reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)));

  // Compare with another IPEndPoint that has a different address.
  IPEndPoint bt_endpoint_other;
  SOCKADDR_BTH addr2 =
      BuildBluetoothSockAddr(kBluetoothAddress2, kBluetoothPort);
  EXPECT_TRUE(bt_endpoint_other.FromSockAddr(
      reinterpret_cast<const struct sockaddr*>(&addr2), sizeof(addr2)));
  EXPECT_LT(bt_endpoint.address(), bt_endpoint_other.address());
  EXPECT_EQ(bt_endpoint.GetFamily(), bt_endpoint_other.GetFamily());
  EXPECT_EQ(bt_endpoint.GetSockAddrFamily(),
            bt_endpoint_other.GetSockAddrFamily());
  // Comparison functions should agree that `bt_endpoint < bt_endpoint_other`
  // due to lexicographic comparison of the address bytes.
  EXPECT_TRUE(bt_endpoint < bt_endpoint_other);
  EXPECT_TRUE(bt_endpoint != bt_endpoint_other);
  EXPECT_FALSE(bt_endpoint == bt_endpoint_other);
  // Test that IPv4/IPv6-only methods crash.
  EXPECT_DCHECK_DEATH(bt_endpoint_other.port());
  SockaddrStorage storage;
  EXPECT_DCHECK_DEATH(std::ignore = bt_endpoint_other.ToSockAddr(
                          storage.addr(), &storage.addr_len));
  EXPECT_DCHECK_DEATH(bt_endpoint_other.ToString());
  EXPECT_DCHECK_DEATH(bt_endpoint_other.ToStringWithoutPort());
}
#endif

TEST_F(IPEndPointTest, Equality) {
  uint16_t port = 0;
  for (const auto& test : tests) {
    IPEndPoint src(test.ip_address, ++port, test.scope_id);
    IPEndPoint dest(src);
    EXPECT_TRUE(src == dest);
  }

  // Compare scope_id.
  const auto v6_link_local_address = *IPAddress::FromIPLiteral("fe80::1");
  IPEndPoint ip_endpoint1 =
      IPEndPoint(v6_link_local_address, 80, /*scope_id=*/1);
  IPEndPoint ip_endpoint2 =
      IPEndPoint(v6_link_local_address, 80, /*scope_id=*/1);
  EXPECT_EQ(ip_endpoint1, ip_endpoint2);
  ip_endpoint2 = IPEndPoint(v6_link_local_address, 80, /*scope_id=*/2);
  EXPECT_NE(ip_endpoint1, ip_endpoint2);
}

TEST_F(IPEndPointTest, LessThan) {
  // Vary by port.
  IPEndPoint ip_endpoint1(tests[0].ip_address, 100);
  IPEndPoint ip_endpoint2(tests[0].ip_address, 1000);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // IPv4 vs IPv6
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 81);
  ip_endpoint2 = IPEndPoint(tests[2].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // IPv4 vs IPv4
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 81);
  ip_endpoint2 = IPEndPoint(tests[1].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // IPv6 vs IPv6
  ip_endpoint1 = IPEndPoint(tests[2].ip_address, 81);
  ip_endpoint2 = IPEndPoint(tests[3].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // Compare equivalent endpoints.
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 80);
  ip_endpoint2 = IPEndPoint(tests[0].ip_address, 80);
  EXPECT_FALSE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);
}

TEST_F(IPEndPointTest, ToString) {
  {
    IPEndPoint endpoint;
    EXPECT_EQ(0, endpoint.port());
  }

  uint16_t port = 100;
  for (const auto& test : tests) {
    ++port;
    IPEndPoint endpoint(test.ip_address, port, test.scope_id);
    const std::string result = endpoint.ToString();
    EXPECT_EQ(test.host_normalized + ":" + base::NumberToString(port), result);
  }

  // ToString() shouldn't crash on invalid addresses.
  IPAddress invalid_address;
  IPEndPoint invalid_endpoint(invalid_address, 8080);
  EXPECT_EQ("", invalid_endpoint.ToString());
  EXPECT_EQ("", invalid_endpoint.ToStringWithoutPort());
}

TEST_F(IPEndPointTest, RoundtripThroughValue) {
  for (const auto& test : tests) {
    IPEndPoint endpoint(test.ip_address, 1645, test.scope_id);
    base::Value value = endpoint.ToValue();

    EXPECT_THAT(IPEndPoint::FromValue(value), Optional(endpoint));
  }
}

TEST_F(IPEndPointTest, FromGarbageValue) {
  base::Value value(123);
  EXPECT_FALSE(IPEndPoint::FromValue(value).has_value());
}

TEST_F(IPEndPointTest, FromMalformedValues) {
  for (const auto& test : tests) {
    base::Value valid_value =
        IPEndPoint(test.ip_address, 1111, test.scope_id).ToValue();
    ASSERT_TRUE(IPEndPoint::FromValue(valid_value).has_value());

    base::Value missing_address = valid_value.Clone();
    ASSERT_TRUE(missing_address.GetDict().Remove("address"));
    EXPECT_FALSE(IPEndPoint::FromValue(missing_address).has_value());

    base::Value missing_port = valid_value.Clone();
    ASSERT_TRUE(missing_port.GetDict().Remove("port"));
    EXPECT_FALSE(IPEndPoint::FromValue(missing_port).has_value());

    base::Value invalid_address = valid_value.Clone();
    *invalid_address.GetDict().Find("address") = base::Value("1.2.3.4.5");
    EXPECT_FALSE(IPEndPoint::FromValue(invalid_address).has_value());

    base::Value negative_port = valid_value.Clone();
    *negative_port.GetDict().Find("port") = base::Value(-1);
    EXPECT_FALSE(IPEndPoint::FromValue(negative_port).has_value());

    base::Value large_port = valid_value.Clone();
    *large_port.GetDict().Find("port") = base::Value(66000);
    EXPECT_FALSE(IPEndPoint::FromValue(large_port).has_value());
  }

  // Invalid values for scope id.
  const auto v6_link_local_address = *IPAddress::FromIPLiteral("fe80::1");
  base::Value valid_value =
      IPEndPoint(v6_link_local_address, /*port=*/80, /*scope_id=*/1).ToValue();

  base::Value invalid_scope_id = valid_value.Clone();
  *invalid_scope_id.GetDict().Find("interface_name") = base::Value("-1");
  EXPECT_FALSE(IPEndPoint::FromValue(invalid_scope_id).has_value());

  base::Value invalid_scope_id2 = valid_value.Clone();
  *invalid_scope_id2.GetDict().Find("interface_name") = base::Value("0");
  EXPECT_FALSE(IPEndPoint::FromValue(invalid_scope_id2).has_value());

  base::Value invalid_address_v4 = valid_value.Clone();
  *invalid_address_v4.GetDict().Find("address") = base::Value("169.254.0.1");
  EXPECT_FALSE(IPEndPoint::FromValue(invalid_scope_id).has_value());

  base::Value invalid_address_v6 = valid_value.Clone();
  *invalid_address_v4.GetDict().Find("address") = base::Value("2001:db8:0::42");
  EXPECT_FALSE(IPEndPoint::FromValue(invalid_scope_id).has_value());

  base::Value invalid_ipv4_mapped_v6_address = valid_value.Clone();
  *invalid_address_v4.GetDict().Find("address") =
      base::Value("::ffff:169.254.0.1");
  EXPECT_FALSE(IPEndPoint::FromValue(invalid_scope_id).has_value());
}

}  // namespace

}  // namespace net

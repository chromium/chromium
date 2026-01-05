// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_getifaddrs.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include <algorithm>
#include <string>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_interfaces_posix.h"
#include "testing/gtest/include/gtest/gtest.h"
#if BUILDFLAG(IS_ANDROID)
#include "net/base/network_interfaces_getifaddrs_android.h"
#endif

namespace net {
namespace {

class IPAttributesGetterTest : public internal::IPAttributesGetter {
 public:
  IPAttributesGetterTest() = default;

  // internal::IPAttributesGetter interface.
  bool IsInitialized() const override { return true; }
  bool GetAddressAttributes(const ifaddrs* if_addr, int* attributes) override {
    *attributes = attributes_;
    return true;
  }
  NetworkChangeNotifier::ConnectionType GetNetworkInterfaceType(
      const ifaddrs* if_addr) override {
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }

  void set_attributes(int attributes) { attributes_ = attributes; }

 private:
  int attributes_ = 0;
};

// Helper function to create a single valid ifaddrs
bool FillIfaddrs(ifaddrs* interfaces,
                 const char* ifname,
                 uint flags,
                 const IPAddress& ip_address,
                 const IPAddress& ip_netmask,
                 base::span<sockaddr_storage> sock_addrs) {
  interfaces->ifa_next = nullptr;
  interfaces->ifa_name = const_cast<char*>(ifname);
  interfaces->ifa_flags = flags;

  socklen_t sock_len = sizeof(sockaddr_storage);

  // Convert to sockaddr for next check.
  if (!IPEndPoint(ip_address, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[0]),
                       &sock_len)) {
    return false;
  }
  interfaces->ifa_addr = reinterpret_cast<sockaddr*>(&sock_addrs[0]);

  sock_len = sizeof(sockaddr_storage);
  if (!IPEndPoint(ip_netmask, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[1]),
                       &sock_len)) {
    return false;
  }
  interfaces->ifa_netmask = reinterpret_cast<sockaddr*>(&sock_addrs[1]);

  return true;
}

static const char kIfnameEm1[] = "em1";
static const char kIfnameVmnet[] = "vmnet";

static const unsigned char kIPv6LocalAddr[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                               0, 0, 0, 0, 0, 0, 0, 1};

// The following 3 addresses need to be changed together. IPv6Addr is the IPv6
// address. IPv6Netmask is the mask address with as many leading bits set to 1
// as the prefix length. IPv6AddrPrefix needs to match IPv6Addr with the same
// number of bits as the prefix length.
static const unsigned char kIPv6Addr[] = {0x24, 0x01, 0xfa, 0x00, 0x00, 0x04,
                                          0x10, 0x00, 0xbe, 0x30, 0x5b, 0xff,
                                          0xfe, 0xe5, 0x00, 0xc3};

static const unsigned char kIPv6Netmask[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00};

TEST(NetworkInterfacesTest, IfaddrsToNetworkInterfaceList) {
  IPAddress ipv6_local_address(kIPv6LocalAddr);
  IPAddress ipv6_address(kIPv6Addr);
  IPAddress ipv6_netmask(kIPv6Netmask);

  NetworkInterfaceList results;
  IPAttributesGetterTest ip_attributes_getter;
  sockaddr_storage addresses[2];
  ifaddrs interface;

  // Address of offline (not running) links should be ignored.
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameEm1, IFF_UP, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 0ul);

  // Address of offline (not up) links should be ignored.
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameEm1, IFF_RUNNING, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 0ul);

  // Local address should be trimmed out.
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameEm1, IFF_UP | IFF_RUNNING,
                          ipv6_local_address, ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 0ul);

  // vmware address should return by default.
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameVmnet, IFF_UP | IFF_RUNNING,
                          ipv6_address, ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameVmnet);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  results.clear();

  // vmware address should be trimmed out if policy specified so.
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameVmnet, IFF_UP | IFF_RUNNING,
                          ipv6_address, ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with banned attributes should be ignored.
  ip_attributes_getter.set_attributes(IP_ADDRESS_ATTRIBUTE_ANYCAST);
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameEm1, IFF_UP | IFF_RUNNING,
                          ipv6_address, ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with allowed attribute IFA_F_TEMPORARY should be returned and
  // attributes should be translated correctly.
  ip_attributes_getter.set_attributes(IP_ADDRESS_ATTRIBUTE_TEMPORARY);
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameEm1, IFF_UP | IFF_RUNNING,
                          ipv6_address, ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameEm1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_TEMPORARY);
  results.clear();

  // Addresses with allowed attribute IFA_F_DEPRECATED should be returned and
  // attributes should be translated correctly.
  ip_attributes_getter.set_attributes(IP_ADDRESS_ATTRIBUTE_DEPRECATED);
  ASSERT_TRUE(FillIfaddrs(&interface, kIfnameEm1, IFF_UP | IFF_RUNNING,
                          ipv6_address, ipv6_netmask, addresses));
  EXPECT_TRUE(internal::IfaddrsToNetworkInterfaceList(
      INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface, &ip_attributes_getter,
      &results));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameEm1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_DEPRECATED);
  results.clear();
}

#if BUILDFLAG(IS_ANDROID)
// Verify that `data.size()` bytes exist at `data.data()` by reading from them.
// Doesn't verify anything about the content of the bytes, just the fact that
// they are readable. Crashes if any of the bytes are not readable.
void CheckBytesExist(base::span<const uint8_t> bytes) {
  // `sink` is volatile to prevent the compiler from optimizing out the copies.
  [[maybe_unused]] volatile uint8_t sink = 0u;
  for (uint8_t byte : bytes) {
    sink = byte;
  }
}

void CheckSockaddrInExists(struct sockaddr* addr) {
  auto* as_in = reinterpret_cast<const sockaddr_in*>(addr);
  CheckBytesExist(base::as_bytes(base::span_from_ref(*as_in)));
}

void CheckSockaddrIn6Exists(struct sockaddr* addr) {
  auto* as_in6 = reinterpret_cast<const sockaddr_in6*>(addr);
  CheckBytesExist(base::as_bytes(base::span_from_ref(*as_in6)));
}

// Helper function to check if a sockaddr represents a valid netmask.
// A valid netmask consists of a contiguous sequence of '1' bits followed by
// a contiguous sequence of '0' bits.
bool IsValidNetmaskAddress(const sockaddr* netmask_sa, int family) {
  CHECK(netmask_sa);

  IPEndPoint netmask_endpoint;
  socklen_t sock_len =
      (family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
  if (!netmask_endpoint.FromSockAddr(netmask_sa, sock_len)) {
    return false;
  }

  const IPAddress& netmask_ip = netmask_endpoint.address();
  // Check the address length.
  if (!netmask_ip.IsValid()) {
    return false;
  }

  // A valid netmask consists of zero or more 1 bits followed by zero or more
  // 0 bits.

  const IPAddressBytes& bytes = netmask_ip.bytes();
  bool seen_0_bit = false;
  for (uint8_t byte : bytes) {
    for (uint8_t bit = 128u; bit != 0; bit >>= 1) {
      if (seen_0_bit && (byte & bit) != 0) {
        return false;
      }
      if ((byte & bit) == 0) {
        seen_0_bit = true;
      }
    }
  }
  return true;
}

// Test the built-in implementation of getifaddrs() named Getifaddrs() that gets
// its information from the kernel via a netlink socket. This is only used on a
// few Android devices where getifaddrs() is known not to work correctly.
//
// This is more like an integration test than a unit test as it uses the real
// values returned by the kernel. As a result, the checks it can perform are
// limited.
TEST(NetworkInterfacesGetifaddrsAndroidTest, Getifaddrs) {
  struct ifaddrs* ifa = nullptr;
  int res = internal::Getifaddrs(&ifa);

  // It's possible for getifaddrs to fail if there are no network interfaces.
  // We can't guarantee that there will be any.
  if (res != 0) {
    ASSERT_EQ(nullptr, ifa);
    // This is not a failure.
    return;
  }

  ASSERT_NE(nullptr, ifa);

  for (struct ifaddrs* cur = ifa; cur != nullptr; cur = cur->ifa_next) {
    ASSERT_NE(nullptr, cur->ifa_name);
    EXPECT_LT(0u, strlen(cur->ifa_name));

    // Check that the interface index is valid.
    auto ifa_index = if_nametoindex(cur->ifa_name);
    EXPECT_NE(0u, ifa_index);

    // The implementation doesn't filter out interfaces that are not up, so we
    // can't assert that IFF_UP is present.

    ASSERT_NE(nullptr, cur->ifa_addr);
    ASSERT_TRUE(cur->ifa_addr->sa_family == AF_INET ||
                cur->ifa_addr->sa_family == AF_INET6);

    // The addresses could be absolutely anything, but they do have to be
    // readable.
    if (cur->ifa_addr->sa_family == AF_INET) {
      CheckSockaddrInExists(cur->ifa_addr);
      CheckSockaddrInExists(cur->ifa_netmask);
    } else {
      CheckSockaddrIn6Exists(cur->ifa_addr);
      CheckSockaddrIn6Exists(cur->ifa_netmask);

      auto* as_in6 = reinterpret_cast<const sockaddr_in6*>(cur->ifa_addr);
      EXPECT_EQ(ifa_index, as_in6->sin6_scope_id);
    }

    ASSERT_NE(nullptr, cur->ifa_netmask);
    EXPECT_EQ(cur->ifa_addr->sa_family, cur->ifa_netmask->sa_family);
    EXPECT_TRUE(
        IsValidNetmaskAddress(cur->ifa_netmask, cur->ifa_addr->sa_family))
        << "Invalid netmask for interface " << cur->ifa_name;

    // The implementation doesn't set these.
    EXPECT_EQ(nullptr, cur->ifa_broadaddr);
    EXPECT_EQ(nullptr, cur->ifa_dstaddr);
    EXPECT_EQ(nullptr, cur->ifa_data);
  }

  internal::Freeifaddrs(ifa);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace net

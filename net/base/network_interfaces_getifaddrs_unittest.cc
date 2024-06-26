// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/network_interfaces_getifaddrs.h"

#include <string>

#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

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
                 sockaddr_storage sock_addrs[2]) {
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

}  // namespace
}  // namespace net

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_linux.h"

#include <net/if.h>
#include <netinet/in.h>

#include <ostream>
#include <string>
#include <unordered_set>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_interfaces_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

static const char kIfnameEm1[] = "em1";
static const char kIfnameVmnet[] = "vmnet";
static const unsigned char kIPv6LocalAddr[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                               0, 0, 0, 0, 0, 0, 0, 1};
static const unsigned char kIPv6Addr[] = {0x24, 0x01, 0xfa, 0x00, 0x00, 0x04,
                                          0x10, 0x00, 0xbe, 0x30, 0x5b, 0xff,
                                          0xfe, 0xe5, 0x00, 0xc3};

char* GetInterfaceName(int interface_index, char* ifname) {
  static_assert(std::size(kIfnameEm1) < IF_NAMESIZE, "Invalid interface name");
  memcpy(ifname, kIfnameEm1, std::size(kIfnameEm1));
  return ifname;
}

char* GetInterfaceNameVM(int interface_index, char* ifname) {
  static_assert(std::size(kIfnameVmnet) < IF_NAMESIZE,
                "Invalid interface name");
  memcpy(ifname, kIfnameVmnet, std::size(kIfnameVmnet));
  return ifname;
}

TEST(NetworkInterfacesTest, NetworkListTrimmingLinux) {
  IPAddress ipv6_local_address(kIPv6LocalAddr);
  IPAddress ipv6_address(kIPv6Addr);

  NetworkInterfaceList results;
  std::unordered_set<int> online_links;
  internal::AddressTrackerLinux::AddressMap address_map;

  // Interface 1 is offline.
  struct ifaddrmsg msg = {
      AF_INET6,         // Address type
      1,                // Prefix length
      IFA_F_TEMPORARY,  // Address flags
      0,                // Link scope
      1                 // Link index
  };

  // Address of offline links should be ignored.
  ASSERT_TRUE(address_map.insert(std::pair(ipv6_address, msg)).second);
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, online_links,
      address_map, GetInterfaceName));
  EXPECT_EQ(results.size(), 0ul);

  // Mark interface 1 online.
  online_links.insert(1);

  // Local address should be trimmed out.
  address_map.clear();
  ASSERT_TRUE(address_map.insert(std::pair(ipv6_local_address, msg)).second);
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, online_links,
      address_map, GetInterfaceName));
  EXPECT_EQ(results.size(), 0ul);

  // vmware address should return by default.
  address_map.clear();
  ASSERT_TRUE(address_map.insert(std::pair(ipv6_address, msg)).second);
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, online_links,
      address_map, GetInterfaceNameVM));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameVmnet);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  results.clear();

  // vmware address should be trimmed out if policy specified so.
  address_map.clear();
  ASSERT_TRUE(address_map.insert(std::pair(ipv6_address, msg)).second);
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, online_links,
      address_map, GetInterfaceNameVM));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with banned attributes should be ignored.
  address_map.clear();
  msg.ifa_flags = IFA_F_TENTATIVE;
  ASSERT_TRUE(address_map.insert(std::pair(ipv6_address, msg)).second);
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, online_links,
      address_map, GetInterfaceName));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with allowed attribute IFA_F_TEMPORARY should be returned and
  // attributes should be translated correctly.
  address_map.clear();
  msg.ifa_flags = IFA_F_TEMPORARY;
  ASSERT_TRUE(address_map.insert(std::pair(ipv6_address, msg)).second);
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, online_links,
      address_map, GetInterfaceName));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameEm1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_TEMPORARY);
  results.clear();

  // Addresses with allowed attribute IFA_F_DEPRECATED should be returned and
  // attributes should be translated correctly.
  address_map.clear();
  msg.ifa_flags = IFA_F_DEPRECATED;
  ASSERT_TRUE(address_map.insert(std::pair(ipv6_address, msg)).second);
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, online_links,
      address_map, GetInterfaceName));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameEm1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_DEPRECATED);
  results.clear();
}

const char kWiFiSSID[] = "TestWiFi";
const char kInterfaceWithDifferentSSID[] = "wlan999";

std::string TestGetInterfaceSSID(const std::string& ifname) {
  return (ifname == kInterfaceWithDifferentSSID) ? "AnotherSSID" : kWiFiSSID;
}

TEST(NetworkInterfacesTest, GetWifiSSIDFromInterfaceList) {
  NetworkInterfaceList list;
  EXPECT_EQ(std::string(), internal::GetWifiSSIDFromInterfaceListInternal(
                               list, TestGetInterfaceSSID));

  NetworkInterface interface1;
  interface1.name = "wlan0";
  interface1.type = NetworkChangeNotifier::CONNECTION_WIFI;
  list.push_back(interface1);
  ASSERT_EQ(1u, list.size());
  EXPECT_EQ(std::string(kWiFiSSID),
            internal::GetWifiSSIDFromInterfaceListInternal(
                list, TestGetInterfaceSSID));

  NetworkInterface interface2;
  interface2.name = "wlan1";
  interface2.type = NetworkChangeNotifier::CONNECTION_WIFI;
  list.push_back(interface2);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(std::string(kWiFiSSID),
            internal::GetWifiSSIDFromInterfaceListInternal(
                list, TestGetInterfaceSSID));

  NetworkInterface interface3;
  interface3.name = kInterfaceWithDifferentSSID;
  interface3.type = NetworkChangeNotifier::CONNECTION_WIFI;
  list.push_back(interface3);
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(std::string(), internal::GetWifiSSIDFromInterfaceListInternal(
                               list, TestGetInterfaceSSID));

  list.pop_back();
  NetworkInterface interface4;
  interface4.name = "eth0";
  interface4.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  list.push_back(interface4);
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(std::string(), internal::GetWifiSSIDFromInterfaceListInternal(
                               list, TestGetInterfaceSSID));
}

}  // namespace

}  // namespace net

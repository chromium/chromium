// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/network_interfaces_win.h"

#include <objbase.h>

#include <iphlpapi.h>

#include <ostream>
#include <string>
#include <unordered_set>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

static const char kIfnameEm1[] = "em1";
static const char kIfnameVmnet[] = "VMnet";

static const unsigned char kIPv6LocalAddr[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                               0, 0, 0, 0, 0, 0, 0, 1};

static const unsigned char kIPv6Addr[] = {0x24, 0x01, 0xfa, 0x00, 0x00, 0x04,
                                          0x10, 0x00, 0xbe, 0x30, 0x5b, 0xff,
                                          0xfe, 0xe5, 0x00, 0xc3};
static const unsigned char kIPv6AddrPrefix[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Helper function to create a valid IP_ADAPTER_ADDRESSES with reasonable
// default value. The output is the |adapter_address|. All the rests are input
// to fill the |adapter_address|. |sock_addrs| are temporary storage used by
// |adapter_address| once the function is returned.
bool FillAdapterAddress(IP_ADAPTER_ADDRESSES* adapter_address,
                        const char* ifname,
                        const IPAddress& ip_address,
                        const IPAddress& ip_netmask,
                        sockaddr_storage sock_addrs[2]) {
  adapter_address->AdapterName = const_cast<char*>(ifname);
  adapter_address->FriendlyName = const_cast<PWCHAR>(L"interface");
  adapter_address->IfType = IF_TYPE_ETHERNET_CSMACD;
  adapter_address->OperStatus = IfOperStatusUp;
  adapter_address->FirstUnicastAddress->DadState = IpDadStatePreferred;
  adapter_address->FirstUnicastAddress->PrefixOrigin = IpPrefixOriginOther;
  adapter_address->FirstUnicastAddress->SuffixOrigin = IpSuffixOriginOther;
  adapter_address->FirstUnicastAddress->PreferredLifetime = 100;
  adapter_address->FirstUnicastAddress->ValidLifetime = 1000;

  DCHECK(sizeof(adapter_address->PhysicalAddress) > 5);
  // Generate 06:05:04:03:02:01
  adapter_address->PhysicalAddressLength = 6;
  for (unsigned long i = 0; i < adapter_address->PhysicalAddressLength; i++) {
    adapter_address->PhysicalAddress[i] =
        adapter_address->PhysicalAddressLength - i;
  }

  socklen_t sock_len = sizeof(sockaddr_storage);

  // Convert to sockaddr for next check.
  if (!IPEndPoint(ip_address, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[0]),
                       &sock_len)) {
    return false;
  }
  adapter_address->FirstUnicastAddress->Address.lpSockaddr =
      reinterpret_cast<sockaddr*>(&sock_addrs[0]);
  adapter_address->FirstUnicastAddress->Address.iSockaddrLength = sock_len;
  adapter_address->FirstUnicastAddress->OnLinkPrefixLength = 1;

  sock_len = sizeof(sockaddr_storage);
  if (!IPEndPoint(ip_netmask, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[1]),
                       &sock_len)) {
    return false;
  }
  adapter_address->FirstPrefix->Address.lpSockaddr =
      reinterpret_cast<sockaddr*>(&sock_addrs[1]);
  adapter_address->FirstPrefix->Address.iSockaddrLength = sock_len;
  adapter_address->FirstPrefix->PrefixLength = 1;

  DCHECK_EQ(sock_addrs[0].ss_family, sock_addrs[1].ss_family);
  if (sock_addrs[0].ss_family == AF_INET6) {
    adapter_address->Ipv6IfIndex = 0;
  } else {
    DCHECK_EQ(sock_addrs[0].ss_family, AF_INET);
    adapter_address->IfIndex = 0;
  }

  return true;
}

TEST(NetworkInterfacesTest, NetworkListTrimmingWindows) {
  IPAddress ipv6_local_address(kIPv6LocalAddr);
  IPAddress ipv6_address(kIPv6Addr);
  IPAddress ipv6_prefix(kIPv6AddrPrefix);

  NetworkInterfaceList results;
  sockaddr_storage addresses[2];
  IP_ADAPTER_ADDRESSES adapter_address = {};
  IP_ADAPTER_UNICAST_ADDRESS address = {};
  IP_ADAPTER_PREFIX adapter_prefix = {};
  adapter_address.FirstUnicastAddress = &address;
  adapter_address.FirstPrefix = &adapter_prefix;

  // Address of offline links should be ignored.
  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameEm1, ipv6_address,
                                 ipv6_prefix, addresses));
  adapter_address.OperStatus = IfOperStatusDown;

  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));

  EXPECT_EQ(results.size(), 0ul);

  // Address on loopback interface should be trimmed out.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, kIfnameEm1 /* ifname */,
      ipv6_local_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  adapter_address.IfType = IF_TYPE_SOFTWARE_LOOPBACK;

  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  EXPECT_EQ(results.size(), 0ul);

  // vmware address should return by default.
  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameVmnet, ipv6_address,
                                 ipv6_prefix, addresses));
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameVmnet);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_NONE);
  results.clear();

  // vmware address should be trimmed out if policy specified so.
  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameVmnet, ipv6_address,
                                 ipv6_prefix, addresses));
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with incomplete DAD should be ignored.
  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameEm1, ipv6_address,
                                 ipv6_prefix, addresses));
  adapter_address.FirstUnicastAddress->DadState = IpDadStateTentative;

  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with allowed attribute IpSuffixOriginRandom should be returned
  // and attributes should be translated correctly to
  // IP_ADDRESS_ATTRIBUTE_TEMPORARY.
  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameEm1, ipv6_address,
                                 ipv6_prefix, addresses));
  adapter_address.FirstUnicastAddress->PrefixOrigin =
      IpPrefixOriginRouterAdvertisement;
  adapter_address.FirstUnicastAddress->SuffixOrigin = IpSuffixOriginRandom;

  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, kIfnameEm1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_TEMPORARY);
  results.clear();

  // Addresses with preferred lifetime 0 should be returned and
  // attributes should be translated correctly to
  // IP_ADDRESS_ATTRIBUTE_DEPRECATED.
  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameEm1, ipv6_address,
                                 ipv6_prefix, addresses));
  adapter_address.FirstUnicastAddress->PreferredLifetime = 0;
  adapter_address.FriendlyName = const_cast<PWCHAR>(L"FriendlyInterfaceName");
  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].friendly_name, "FriendlyInterfaceName");
  EXPECT_EQ(results[0].name, kIfnameEm1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_DEPRECATED);
  results.clear();
}

TEST(NetworkInterfacesTest, NetworkListExtractMacAddress) {
  IPAddress ipv6_local_address(kIPv6LocalAddr);
  IPAddress ipv6_address(kIPv6Addr);
  IPAddress ipv6_prefix(kIPv6AddrPrefix);

  NetworkInterfaceList results;
  sockaddr_storage addresses[2];
  IP_ADAPTER_ADDRESSES adapter_address = {};
  IP_ADAPTER_UNICAST_ADDRESS address = {};
  IP_ADAPTER_PREFIX adapter_prefix = {};
  adapter_address.FirstUnicastAddress = &address;
  adapter_address.FirstPrefix = &adapter_prefix;

  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameEm1, ipv6_address,
                                 ipv6_prefix, addresses));

  Eui48MacAddress expected_mac_address = {0x6, 0x5, 0x4, 0x3, 0x2, 0x1};

  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  ASSERT_EQ(results.size(), 1ul);
  ASSERT_EQ(results[0].mac_address, expected_mac_address);
}

TEST(NetworkInterfacesTest, NetworkListExtractMacAddressInvalidLength) {
  IPAddress ipv6_local_address(kIPv6LocalAddr);
  IPAddress ipv6_address(kIPv6Addr);
  IPAddress ipv6_prefix(kIPv6AddrPrefix);

  NetworkInterfaceList results;
  sockaddr_storage addresses[2];
  IP_ADAPTER_ADDRESSES adapter_address = {};
  IP_ADAPTER_UNICAST_ADDRESS address = {};
  IP_ADAPTER_PREFIX adapter_prefix = {};
  adapter_address.FirstUnicastAddress = &address;
  adapter_address.FirstPrefix = &adapter_prefix;

  ASSERT_TRUE(FillAdapterAddress(&adapter_address, kIfnameEm1, ipv6_address,
                                 ipv6_prefix, addresses));
  // Not EUI-48 Mac address, so it is not extracted.
  adapter_address.PhysicalAddressLength = 8;

  EXPECT_TRUE(internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &adapter_address));
  ASSERT_EQ(results.size(), 1ul);
  EXPECT_FALSE(results[0].mac_address.has_value());
}

bool read_int_or_bool(DWORD data_size, PVOID data) {
  switch (data_size) {
    case 1:
      return !!*reinterpret_cast<uint8_t*>(data);
    case 4:
      return !!*reinterpret_cast<uint32_t*>(data);
    default:
      LOG(FATAL) << "That is not a type I know!";
  }
}

int GetWifiOptions() {
  const internal::WlanApi& wlanapi = internal::WlanApi::GetInstance();
  if (!wlanapi.initialized)
    return -1;

  internal::WlanHandle client;
  DWORD cur_version = 0;
  const DWORD kMaxClientVersion = 2;
  DWORD result = wlanapi.OpenHandle(kMaxClientVersion, &cur_version, &client);
  if (result != ERROR_SUCCESS)
    return -1;

  WLAN_INTERFACE_INFO_LIST* interface_list_ptr = nullptr;
  result =
      wlanapi.enum_interfaces_func(client.Get(), nullptr, &interface_list_ptr);
  if (result != ERROR_SUCCESS)
    return -1;
  std::unique_ptr<WLAN_INTERFACE_INFO_LIST, internal::WlanApiDeleter>
      interface_list(interface_list_ptr);

  for (unsigned i = 0; i < interface_list->dwNumberOfItems; ++i) {
    WLAN_INTERFACE_INFO* info = &interface_list->InterfaceInfo[i];
    DWORD data_size;
    PVOID data;
    int options = 0;
    result =
        wlanapi.query_interface_func(client.Get(), &info->InterfaceGuid,
                                     wlan_intf_opcode_background_scan_enabled,
                                     nullptr, &data_size, &data, nullptr);
    if (result != ERROR_SUCCESS)
      continue;
    if (!read_int_or_bool(data_size, data)) {
      options |= WIFI_OPTIONS_DISABLE_SCAN;
    }
    internal::WlanApi::GetInstance().free_memory_func(data);

    result = wlanapi.query_interface_func(client.Get(), &info->InterfaceGuid,
                                          wlan_intf_opcode_media_streaming_mode,
                                          nullptr, &data_size, &data, nullptr);
    if (result != ERROR_SUCCESS)
      continue;
    if (read_int_or_bool(data_size, data)) {
      options |= WIFI_OPTIONS_MEDIA_STREAMING_MODE;
    }
    internal::WlanApi::GetInstance().free_memory_func(data);

    // Just the the options from the first succesful
    // interface.
    return options;
  }

  // No wifi interface found.
  return -1;
}

void TryChangeWifiOptions(int options) {
  int previous_options = GetWifiOptions();
  std::unique_ptr<ScopedWifiOptions> scoped_options = SetWifiOptions(options);
  EXPECT_EQ(previous_options | options, GetWifiOptions());
  scoped_options.reset();
  EXPECT_EQ(previous_options, GetWifiOptions());
}

// Test fails on Win Arm64 bots. TODO(crbug.com/40260910): Fix on bot.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_SetWifiOptions DISABLED_SetWifiOptions
#else
#define MAYBE_SetWifiOptions SetWifiOptions
#endif
// Test SetWifiOptions().
TEST(NetworkInterfacesTest, MAYBE_SetWifiOptions) {
  TryChangeWifiOptions(0);
  TryChangeWifiOptions(WIFI_OPTIONS_DISABLE_SCAN);
  TryChangeWifiOptions(WIFI_OPTIONS_MEDIA_STREAMING_MODE);
  TryChangeWifiOptions(WIFI_OPTIONS_DISABLE_SCAN |
                       WIFI_OPTIONS_MEDIA_STREAMING_MODE);
}

}  // namespace

}  // namespace net

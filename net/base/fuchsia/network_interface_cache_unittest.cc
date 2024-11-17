// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/fuchsia/network_interface_cache.h"

#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <fuchsia/net/interfaces/cpp/fidl_test_base.h>

#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::internal {
namespace {

enum : InterfaceProperties::InterfaceId {
  kDefaultInterfaceId = 1,
  kSecondaryInterfaceId = 2
};

using IPv4Octets = std::array<uint8_t, 4>;

constexpr IPv4Octets kDefaultIPv4Address = {192, 168, 0, 2};
constexpr uint8_t kDefaultIPv4Prefix = 16;

constexpr const char kDefaultInterfaceName[] = "net1";

fuchsia::net::IpAddress IpAddressFrom(IPv4Octets octets) {
  fuchsia::net::IpAddress output;
  output.ipv4().addr = octets;
  return output;
}

template <typename T>
fuchsia::net::Subnet SubnetFrom(T octets, uint8_t prefix) {
  fuchsia::net::Subnet output;
  output.addr = IpAddressFrom(octets);
  output.prefix_len = prefix;
  return output;
}

template <typename T>
fuchsia::net::interfaces::Address InterfaceAddressFrom(T octets,
                                                       uint8_t prefix) {
  fuchsia::net::interfaces::Address addr;
  addr.set_addr(SubnetFrom(octets, prefix));
  return addr;
}

template <typename T>
std::vector<T> MakeSingleItemVec(T item) {
  std::vector<T> vec;
  vec.push_back(std::move(item));
  return vec;
}

fuchsia::net::interfaces::Properties DefaultInterfaceProperties(
    fuchsia::hardware::network::PortClass device_class =
        fuchsia::hardware::network::PortClass::ETHERNET) {
  // For most tests a live interface with an IPv4 address and ethernet class is
  // sufficient.
  fuchsia::net::interfaces::Properties properties;
  properties.set_id(kDefaultInterfaceId);
  properties.set_name(kDefaultInterfaceName);
  properties.set_online(true);
  properties.set_has_default_ipv4_route(true);
  properties.set_has_default_ipv6_route(false);
  properties.set_port_class(fuchsia::net::interfaces::PortClass::WithDevice(
      std::move(device_class)));
  properties.set_addresses(MakeSingleItemVec(
      InterfaceAddressFrom(kDefaultIPv4Address, kDefaultIPv4Prefix)));
  return properties;
}

}  // namespace

class NetworkInterfaceCacheTest : public testing::Test {};

TEST_F(NetworkInterfaceCacheTest, AddInterface) {
  NetworkInterfaceCache cache(false);

  auto change_bits = cache.AddInterface(DefaultInterfaceProperties());

  ASSERT_TRUE(change_bits.has_value());
  EXPECT_EQ(change_bits.value(),
            NetworkInterfaceCache::kIpAddressChanged |
                NetworkInterfaceCache::kConnectionTypeChanged);

  NetworkInterfaceList networks;
  EXPECT_TRUE(cache.GetOnlineInterfaces(&networks));
  EXPECT_EQ(networks.size(), 1u);

  EXPECT_EQ(cache.GetConnectionType(),
            NetworkChangeNotifier::CONNECTION_ETHERNET);
}

TEST_F(NetworkInterfaceCacheTest, RemoveInterface) {
  NetworkInterfaceCache cache(false);
  cache.AddInterface(DefaultInterfaceProperties());

  auto change_bits = cache.RemoveInterface(kDefaultInterfaceId);

  ASSERT_TRUE(change_bits.has_value());
  EXPECT_EQ(change_bits.value(),
            NetworkInterfaceCache::kIpAddressChanged |
                NetworkInterfaceCache::kConnectionTypeChanged);

  NetworkInterfaceList networks;
  EXPECT_TRUE(cache.GetOnlineInterfaces(&networks));
  EXPECT_EQ(networks.size(), 0u);

  EXPECT_EQ(cache.GetConnectionType(), NetworkChangeNotifier::CONNECTION_NONE);
}

TEST_F(NetworkInterfaceCacheTest, ChangeInterface) {
  NetworkInterfaceCache cache(false);
  cache.AddInterface(DefaultInterfaceProperties());

  fuchsia::net::interfaces::Properties properties;
  properties.set_id(kDefaultInterfaceId);
  properties.set_port_class(
      fuchsia::net::interfaces::PortClass::WithLoopback(
          fuchsia::net::interfaces::Empty()));
  properties.set_addresses({});

  auto change_bits = cache.ChangeInterface(std::move(properties));

  ASSERT_TRUE(change_bits.has_value());
  EXPECT_EQ(change_bits.value(),
            NetworkInterfaceCache::kIpAddressChanged |
                NetworkInterfaceCache::kConnectionTypeChanged);

  NetworkInterfaceList networks;
  EXPECT_TRUE(cache.GetOnlineInterfaces(&networks));
  EXPECT_EQ(networks.size(), 0u);

  EXPECT_EQ(cache.GetConnectionType(), NetworkChangeNotifier::CONNECTION_NONE);
}

// TODO(crbug.com/40721278): Add more tests that exercise different error
// states.

}  // namespace net::internal

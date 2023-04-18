// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_interface_change_listener_mojom_traits.h"

#include <linux/if_addr.h>

#include <unordered_set>
#include <vector>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux_test_util.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/network_interface_change_listener.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

TEST(NetworkInterfaceChangeListenerMojomTraitsTest, IfAddrMsg) {
  struct ifaddrmsg original_msg = {
      .ifa_family = 1,
      .ifa_prefixlen = 2,
      .ifa_flags = 3,
      .ifa_scope = 4,
      .ifa_index = 5,
  };
  struct ifaddrmsg copied_msg;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::IfAddrMsg>(
      original_msg, copied_msg));
  EXPECT_EQ(original_msg, copied_msg);
}

TEST(NetworkInterfaceChangeListenerMojomTraitsTest, OnlineLinks) {
  std::unordered_set<int> original_online_links = {1, 2, 3};
  std::unordered_set<int> copied_online_links;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::OnlineLinks>(
      original_online_links, copied_online_links));
  EXPECT_EQ(original_online_links, copied_online_links);
}

TEST(NetworkInterfaceChangeListenerMojomTraitsTest, AddressMap) {
  static constexpr uint8_t addr1[4] = {192, 168, 0, 1};
  static constexpr uint8_t addr2[16] = {0xFE, 0xDC, 0xBA, 0x98};
  net::IPAddress ip_address1(addr1);
  net::IPAddress ip_address2(addr2);
  struct ifaddrmsg msg1 = {
      .ifa_family = 1,
      .ifa_prefixlen = 2,
      .ifa_flags = 3,
      .ifa_scope = 4,
      .ifa_index = 5,
  };
  struct ifaddrmsg msg2 = {
      .ifa_family = 6,
      .ifa_prefixlen = 7,
      .ifa_flags = 8,
      .ifa_scope = 9,
      .ifa_index = 10,
  };
  net::AddressMapOwnerLinux::AddressMap original_address_map = {
      {ip_address1, msg1}, {ip_address2, msg2}};

  net::AddressMapOwnerLinux::AddressMap copied_address_map;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::AddressMap>(
      original_address_map, copied_address_map));
  EXPECT_EQ(original_address_map, copied_address_map);
}

}  // namespace mojo

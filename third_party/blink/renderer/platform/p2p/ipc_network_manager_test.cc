// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/ipc_network_manager.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/p2p/network_list_manager.h"
#include "third_party/webrtc/rtc_base/mdns_responder_interface.h"

namespace blink {

namespace {

class MockP2PSocketDispatcher
    : public GarbageCollected<MockP2PSocketDispatcher>,
      public NetworkListManager {
 public:
  void AddNetworkListObserver(
      blink::NetworkListObserver* network_list_observer) override {}

  void RemoveNetworkListObserver(
      blink::NetworkListObserver* network_list_observer) override {}

  void Trace(Visitor* visitor) const override {
    NetworkListManager::Trace(visitor);
  }
};

class EmptyMdnsResponder : public webrtc::MdnsResponderInterface {
 public:
  void CreateNameForAddress(const rtc::IPAddress& addr,
                            NameCreatedCallback callback) override {}
  void RemoveNameForAddress(const rtc::IPAddress& addr,
                            NameRemovedCallback callback) override {}
};

}  // namespace

// 2 IPv6 addresses with only last digit different.
static const char kIPv6PublicAddrString1[] =
    "2401:fa00:4:1000:be30:5b30:50e5:c3";
static const char kIPv6PublicAddrString2[] =
    "2401:fa00:4:1000:be30:5b30:50e5:c4";
static const char kIPv4MappedAddrString[] = "::ffff:38.32.0.0";

class IpcNetworkManagerTest : public testing::Test {
 public:
  IpcNetworkManagerTest()
      : network_list_manager_(MakeGarbageCollected<MockP2PSocketDispatcher>()),
        network_manager_(std::make_unique<IpcNetworkManager>(
            network_list_manager_.Get(),
            std::make_unique<EmptyMdnsResponder>())) {}

  ~IpcNetworkManagerTest() override { network_manager_->ContextDestroyed(); }

 protected:
  Persistent<MockP2PSocketDispatcher> network_list_manager_;
  std::unique_ptr<IpcNetworkManager> network_manager_;
};

// Test overall logic of IpcNetworkManager on OnNetworkListChanged
// that it should group addresses with the same network key under
// single Network class. This also tests the logic inside
// IpcNetworkManager in addition to MergeNetworkList.
// TODO(guoweis): disable this test case for now until fix for webrtc
// issue 19249005 integrated into chromium
TEST_F(IpcNetworkManagerTest, TestMergeNetworkList) {
  net::NetworkInterfaceList list;
  net::IPAddress ip;
  rtc::IPAddress ip_address;

  // Add 2 networks with the same prefix and prefix length.
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString1));
  list.push_back(net::NetworkInterface(
      "em1", "em1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString2));
  list.push_back(net::NetworkInterface(
      "em1", "em1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  network_manager_->OnNetworkListChanged(list, net::IPAddress(),
                                         net::IPAddress());
  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();
  EXPECT_EQ(1uL, networks.size());
  EXPECT_EQ(2uL, networks[0]->GetIPs().size());

  // Add another network with different prefix length, should result in
  // a different network.
  list.push_back(net::NetworkInterface(
      "em1", "em1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 48,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  // Push an unknown address as the default address.
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv4MappedAddrString));
  network_manager_->OnNetworkListChanged(list, net::IPAddress(), ip);

  // The unknown default address should be ignored.
  EXPECT_FALSE(network_manager_->GetDefaultLocalAddress(AF_INET6, &ip_address));

  networks = network_manager_->GetNetworks();

  // Verify we have 2 networks now.
  EXPECT_EQ(2uL, networks.size());
  // Verify the network with prefix length of 64 has 2 IP addresses.
  auto network_with_two_ips =
      base::ranges::find(networks, 64, &rtc::Network::prefix_length);
  ASSERT_NE(networks.end(), network_with_two_ips);
  EXPECT_EQ(2uL, (*network_with_two_ips)->GetIPs().size());
  // IPs should be in the same order as the list passed into
  // OnNetworkListChanged.
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString1, &ip_address));
  EXPECT_EQ((*network_with_two_ips)->GetIPs()[0],
            rtc::InterfaceAddress(ip_address));
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString2, &ip_address));
  EXPECT_EQ((*network_with_two_ips)->GetIPs()[1],
            rtc::InterfaceAddress(ip_address));
  // Verify the network with prefix length of 48 has 1 IP address.
  auto network_with_one_ip =
      base::ranges::find(networks, 48, &rtc::Network::prefix_length);
  ASSERT_NE(networks.end(), network_with_one_ip);
  EXPECT_EQ(1uL, (*network_with_one_ip)->GetIPs().size());
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString2, &ip_address));
  EXPECT_EQ((*network_with_one_ip)->GetIPs()[0],
            rtc::InterfaceAddress(ip_address));
}

// Test that IpcNetworkManager will guess a network type from the interface
// name when not otherwise available.
TEST_F(IpcNetworkManagerTest, DeterminesNetworkTypeFromNameIfUnknown) {
  net::NetworkInterfaceList list;
  net::IPAddress ip;
  rtc::IPAddress ip_address;

  // Add a "tun1" entry of type "unknown" and "tun2" entry of type Wi-Fi. The
  // "tun1" entry (and only it) should have its type determined from its name,
  // since its type is unknown.
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString1));
  list.push_back(net::NetworkInterface(
      "tun1", "tun1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString2));
  list.push_back(net::NetworkInterface(
      "tun2", "tun2", 0, net::NetworkChangeNotifier::CONNECTION_WIFI, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  network_manager_->OnNetworkListChanged(list, net::IPAddress(),
                                         net::IPAddress());
  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();
  EXPECT_EQ(2uL, networks.size());

  auto tun1 = base::ranges::find(networks, "tun1", &rtc::Network::name);
  ASSERT_NE(networks.end(), tun1);
  auto tun2 = base::ranges::find(networks, "tun2", &rtc::Network::name);
  ASSERT_NE(networks.end(), tun1);

  EXPECT_EQ(rtc::ADAPTER_TYPE_VPN, (*tun1)->type());
  EXPECT_EQ(rtc::ADAPTER_TYPE_WIFI, (*tun2)->type());
}

// Test that IpcNetworkManager will detect hardcoded VPN interfaces.
TEST_F(IpcNetworkManagerTest, DeterminesVPNFromMacAddress) {
  net::NetworkInterfaceList list;
  net::IPAddress ip;
  rtc::IPAddress ip_address;
  std::optional<net::Eui48MacAddress> mac_address(
      {0x0, 0x5, 0x9A, 0x3C, 0x7A, 0x0});

  // Assign the magic MAC address known to be a Cisco Anyconnect VPN interface.
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString1));
  list.push_back(net::NetworkInterface(
      "eth0", "eth1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE, mac_address));

  network_manager_->OnNetworkListChanged(list, net::IPAddress(),
                                         net::IPAddress());
  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();
  ASSERT_EQ(1uL, networks.size());
  ASSERT_EQ(rtc::ADAPTER_TYPE_VPN, networks[0]->type());
  ASSERT_EQ(rtc::ADAPTER_TYPE_UNKNOWN, networks[0]->underlying_type_for_vpn());
}

// Test that IpcNetworkManager doesn't classify this mac as VPN.
TEST_F(IpcNetworkManagerTest, DeterminesNotVPN) {
  net::NetworkInterfaceList list;
  net::IPAddress ip;
  rtc::IPAddress ip_address;
  std::optional<net::Eui48MacAddress> mac_address(
      {0x0, 0x5, 0x9A, 0x3C, 0x7A, 0x1});

  // This is close to a magic VPN mac but shouldn't match.
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString1));
  list.push_back(net::NetworkInterface(
      "eth0", "eth1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE, mac_address));

  network_manager_->OnNetworkListChanged(list, net::IPAddress(),
                                         net::IPAddress());
  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();
  ASSERT_EQ(1uL, networks.size());
  ASSERT_EQ(rtc::ADAPTER_TYPE_ETHERNET, networks[0]->type());
}

// Test that IpcNetworkManager will act as the mDNS responder provider for
// all networks that it returns.
TEST_F(IpcNetworkManagerTest,
       ServeAsMdnsResponderProviderForNetworksEnumerated) {
  net::NetworkInterfaceList list;
  // Add networks.
  net::IPAddress ip;
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString1));
  list.push_back(net::NetworkInterface(
      "em1", "em1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  network_manager_->OnNetworkListChanged(list, net::IPAddress(),
                                         net::IPAddress());
  std::vector<const rtc::Network*> networks = network_manager_->GetNetworks();

  ASSERT_EQ(1u, networks.size());
  webrtc::MdnsResponderInterface* const mdns_responder =
      network_manager_->GetMdnsResponder();
  EXPECT_EQ(mdns_responder, networks[0]->GetMdnsResponder());
  networks = network_manager_->GetAnyAddressNetworks();
  ASSERT_EQ(2u, networks.size());
  EXPECT_EQ(mdns_responder, networks[0]->GetMdnsResponder());
  EXPECT_EQ(mdns_responder, networks[1]->GetMdnsResponder());
}

}  // namespace blink

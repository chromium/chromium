// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Note: This test is subject to the host's OS and network connection. This test
// is not future-proof. New standards will come about necessitating the need to
// alter the ranges of these tests.
TEST(NetworkChangeNotifierTest, NetMaxBandwidthRange) {
  NetworkChangeNotifier::ConnectionType connection_type =
      NetworkChangeNotifier::CONNECTION_NONE;
  double max_bandwidth = 0.0;
  NetworkChangeNotifier::GetMaxBandwidthAndConnectionType(&max_bandwidth,
                                                          &connection_type);

  // Always accept infinity as it's the default value if the bandwidth is
  // unknown.
  if (max_bandwidth == std::numeric_limits<double>::infinity()) {
    EXPECT_NE(NetworkChangeNotifier::CONNECTION_NONE, connection_type);
    return;
  }

  switch (connection_type) {
    case NetworkChangeNotifier::CONNECTION_UNKNOWN:
      EXPECT_EQ(std::numeric_limits<double>::infinity(), max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_ETHERNET:
      EXPECT_GE(10.0, max_bandwidth);
      EXPECT_LE(10000.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_WIFI:
      EXPECT_GE(1.0, max_bandwidth);
      EXPECT_LE(7000.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_2G:
      EXPECT_GE(0.01, max_bandwidth);
      EXPECT_LE(0.384, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_3G:
      EXPECT_GE(2.0, max_bandwidth);
      EXPECT_LE(42.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_4G:
      EXPECT_GE(100.0, max_bandwidth);
      EXPECT_LE(100.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_NONE:
      EXPECT_EQ(0.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      EXPECT_GE(1.0, max_bandwidth);
      EXPECT_LE(24.0, max_bandwidth);
      break;
  }
}

TEST(NetworkChangeNotifierTest, ConnectionTypeFromInterfaceList) {
  NetworkInterfaceList list;

  // Test empty list.
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
            NetworkChangeNotifier::CONNECTION_NONE);

  for (int i = NetworkChangeNotifier::CONNECTION_UNKNOWN;
       i <= NetworkChangeNotifier::CONNECTION_LAST; i++) {
    // Check individual types.
    NetworkInterface interface;
    interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(i);
    list.clear();
    list.push_back(interface);
    EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list), i);
    // Check two types.
    for (int j = NetworkChangeNotifier::CONNECTION_UNKNOWN;
         j <= NetworkChangeNotifier::CONNECTION_LAST; j++) {
      list.clear();
      interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(i);
      list.push_back(interface);
      interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(j);
      list.push_back(interface);
      EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
                i == j ? i : NetworkChangeNotifier::CONNECTION_UNKNOWN);
    }
  }
}

TEST(NetworkChangeNotifierTest, IgnoreTeredoOnWindows) {
  NetworkInterfaceList list;
  NetworkInterface interface_teredo;
  interface_teredo.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  interface_teredo.friendly_name = "Teredo Tunneling Pseudo-Interface";
  list.push_back(interface_teredo);

#if defined(OS_WIN)
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#else
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_ETHERNET,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#endif
}

TEST(NetworkChangeNotifierTest, IgnoreAirdropOnMac) {
  NetworkInterfaceList list;
  NetworkInterface interface_airdrop;
  interface_airdrop.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  interface_airdrop.name = "awdl0";
  interface_airdrop.friendly_name = "awdl0";
  interface_airdrop.address =
      // Link-local IPv6 address
      IPAddress({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4});
  list.push_back(interface_airdrop);

#if defined(OS_MACOSX)
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#else
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_ETHERNET,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#endif
}

TEST(NetworkChangeNotifierTest, IgnoreTunnelsOnMac) {
  NetworkInterfaceList list;
  NetworkInterface interface_tunnel;
  interface_tunnel.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  interface_tunnel.name = "utun0";
  interface_tunnel.friendly_name = "utun0";
  interface_tunnel.address =
      // Link-local IPv6 address
      IPAddress({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 3, 2, 1});
  list.push_back(interface_tunnel);

#if defined(OS_MACOSX)
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#else
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_ETHERNET,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#endif
}

TEST(NetworkChangeNotifierTest, IgnoreDisconnectedEthernetOnMac) {
  NetworkInterfaceList list;
  NetworkInterface interface_ethernet;
  interface_ethernet.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  interface_ethernet.name = "en5";
  interface_ethernet.friendly_name = "en5";
  interface_ethernet.address =
      // Link-local IPv6 address
      IPAddress({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 1, 2, 3});
  list.push_back(interface_ethernet);

#if defined(OS_MACOSX)
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#else
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_ETHERNET,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
#endif
}

TEST(NetworkChangeNotifierTest, IgnoreVMInterfaces) {
  NetworkInterfaceList list;
  NetworkInterface interface_vmnet_linux;
  interface_vmnet_linux.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  interface_vmnet_linux.name = "vmnet1";
  interface_vmnet_linux.friendly_name = "vmnet1";
  list.push_back(interface_vmnet_linux);

  NetworkInterface interface_vmnet_win;
  interface_vmnet_win.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  interface_vmnet_win.name = "virtualdevice";
  interface_vmnet_win.friendly_name = "VMware Network Adapter VMnet1";
  list.push_back(interface_vmnet_win);

  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list));
}

TEST(NetworkChangeNotifierTest, GetConnectionSubtype) {
  // Call GetConnectionSubtype() and ensure that there is no crash.
  NetworkChangeNotifier::GetConnectionSubtype();
}

class NetworkChangeNotifierMockedTest : public TestWithTaskEnvironment {
 private:
  test::ScopedMockNetworkChangeNotifier mock_notifier_;
};

class TestDnsObserver : public NetworkChangeNotifier::DNSObserver {
 public:
  void OnDNSChanged() override { ++dns_changed_calls_; }

  int dns_changed_calls() const { return dns_changed_calls_; }

 private:
  int dns_changed_calls_ = 0;
};

TEST_F(NetworkChangeNotifierMockedTest, TriggerNonSystemDnsChange) {
  TestDnsObserver observer;
  NetworkChangeNotifier::AddDNSObserver(&observer);

  ASSERT_EQ(0, observer.dns_changed_calls());

  NetworkChangeNotifier::TriggerNonSystemDnsChange();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.dns_changed_calls());

  NetworkChangeNotifier::RemoveDNSObserver(&observer);
}

}  // namespace net

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const int kDefaultNic = 1;
const int kSecondaryNic = kDefaultNic + 1;

fuchsia::netstack::NetAddress CreateIPv6Address(std::vector<uint8_t> addr) {
  fuchsia::netstack::NetAddress output;
  output.family = fuchsia::netstack::NetAddressFamily::IPV6;
  output.ipv6 = fuchsia::netstack::Ipv6Address::New();
  for (size_t i = 0; i < addr.size(); ++i) {
    output.ipv6->addr[i] = addr[i];
  }
  return output;
}

fuchsia::netstack::Subnet CreateSubnet(const std::vector<uint8_t>& addr,
                                       uint8_t prefix) {
  fuchsia::netstack::Subnet output;
  output.addr = CreateIPv6Address(addr);
  output.prefix_len = prefix;
  return output;
}

fuchsia::netstack::NetAddress CreateIPv4Address(uint8_t a0,
                                                uint8_t a1,
                                                uint8_t a2,
                                                uint8_t a3) {
  fuchsia::netstack::NetAddress output;
  output.family = fuchsia::netstack::NetAddressFamily::IPV4;
  output.ipv4 = std::make_unique<fuchsia::netstack::Ipv4Address>();
  output.ipv4->addr[0] = a0;
  output.ipv4->addr[1] = a1;
  output.ipv4->addr[2] = a2;
  output.ipv4->addr[3] = a3;
  return output;
}

fuchsia::netstack::RouteTableEntry CreateRouteTableEntry(uint32_t nicid,
                                                         bool is_default) {
  fuchsia::netstack::RouteTableEntry output;
  output.nicid = nicid;

  if (is_default) {
    output.netmask = CreateIPv4Address(0, 0, 0, 0);
  } else {
    output.netmask = CreateIPv4Address(255, 255, 255, 0);
  }

  return output;
}

fuchsia::netstack::NetInterface CreateNetInterface(
    uint32_t id,
    uint32_t flags,
    uint32_t features,
    fuchsia::netstack::NetAddress address,
    fuchsia::netstack::NetAddress netmask,
    std::vector<fuchsia::netstack::Subnet> ipv6) {
  fuchsia::netstack::NetInterface output;
  output.name = "foo";
  output.id = id;
  output.flags = flags;
  output.features = features;
  address.Clone(&output.addr);
  netmask.Clone(&output.netmask);
  output.hwaddr = fidl::VectorPtr<uint8_t>::New(0);

  output.ipv6addrs =
      fidl::VectorPtr<fuchsia::netstack::Subnet>::New(ipv6.size());
  for (auto& x : ipv6) {
    output.ipv6addrs.push_back(std::move(x));
  }

  return output;
}

// Partial fake implementation of a Netstack.
// GMock is not used because the methods make heavy use of move-only datatypes,
// which aren't handled well by GMock.
class FakeNetstack : public fuchsia::netstack::Netstack {
 public:
  explicit FakeNetstack(
      fidl::InterfaceRequest<fuchsia::netstack::Netstack> netstack_request)
      : binding_(this) {
    CHECK_EQ(ZX_OK, binding_.Bind(std::move(netstack_request)));
  }
  ~FakeNetstack() override = default;

  // Adds |interface| to the interface query response list.
  void PushInterface(fuchsia::netstack::NetInterface&& interface) {
    interfaces_->push_back(std::move(interface));
  }

  void PushRouteTableEntry(fuchsia::netstack::RouteTableEntry&& interface) {
    route_table_->push_back(std::move(interface));
  }

  // Sends the accumulated |interfaces_| to the OnInterfacesChanged event.
  void NotifyInterfaces() {
    binding_.events().OnInterfacesChanged(std::move(interfaces_));
    interfaces_ = fidl::VectorPtr<fuchsia::netstack::NetInterface>::New(0);
  }

  fidl::Binding<fuchsia::netstack::Netstack>& binding() { return binding_; }

 private:
  void GetInterfaces(GetInterfacesCallback callback) override {
    callback(std::move(interfaces_));
  }

  void GetRouteTable(GetRouteTableCallback callback) override {
    fidl::VectorPtr<fuchsia::netstack::RouteTableEntry> table =
        fidl::VectorPtr<fuchsia::netstack::RouteTableEntry>::New(2);
    (*table)[0].nicid = kDefaultNic;
    (*table)[0].netmask = CreateIPv4Address(0, 0, 0, 0);
    (*table)[1].nicid = kSecondaryNic;
    (*table)[1].netmask = CreateIPv4Address(255, 255, 255, 0);

    callback(std::move(table));
  }

  // No-op stubs for the methods we don't care about.
  void GetPortForService(::fidl::StringPtr service,
                         fuchsia::netstack::Protocol protocol,
                         GetPortForServiceCallback callback) override {}
  void GetAddress(::fidl::StringPtr address,
                  uint16_t port,
                  GetAddressCallback callback) override {}
  void GetStats(uint32_t nicid, GetStatsCallback callback) override {}
  void GetAggregateStats(GetAggregateStatsCallback callback) override {}
  void SetInterfaceStatus(uint32_t nicid, bool enabled) override {}
  void SetInterfaceAddress(uint32_t nicid,
                           fuchsia::netstack::NetAddress addr,
                           uint8_t prefixLen,
                           SetInterfaceAddressCallback callback) override {}
  void RemoveInterfaceAddress(
      uint32_t nicid,
      fuchsia::netstack::NetAddress addr,
      uint8_t prefixLen,
      RemoveInterfaceAddressCallback callback) override {}
  void SetDhcpClientStatus(uint32_t nicid,
                           bool enabled,
                           SetDhcpClientStatusCallback callback) override {}
  void BridgeInterfaces(::fidl::VectorPtr<uint32_t> nicids,
                        BridgeInterfacesCallback callback) override {}
  void SetNameServers(
      ::fidl::VectorPtr<::fuchsia::netstack::NetAddress> servers) override {}
  void AddEthernetDevice(
      ::fidl::StringPtr topological_path,
      fuchsia::netstack::InterfaceConfig interfaceConfig,
      ::fidl::InterfaceHandle<::zircon::ethernet::Device> device) override {}
  void StartRouteTableTransaction(
      ::fidl::InterfaceRequest<::fuchsia::netstack::RouteTableTransaction>
          routeTableTransaction,
      StartRouteTableTransactionCallback callback) override {}

  ::fidl::VectorPtr<fuchsia::netstack::NetInterface> interfaces_ =
      fidl::VectorPtr<fuchsia::netstack::NetInterface>::New(0);
  ::fidl::VectorPtr<fuchsia::netstack::RouteTableEntry> route_table_ =
      fidl::VectorPtr<fuchsia::netstack::RouteTableEntry>::New(0);

  fidl::Binding<fuchsia::netstack::Netstack> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetstack);
};

class MockNetworkChangeObserver
    : public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  MOCK_METHOD1(OnNetworkChanged, void(NetworkChangeNotifier::ConnectionType));
};

}  // namespace

class NetworkChangeNotifierFuchsiaTest : public testing::Test {
 public:
  NetworkChangeNotifierFuchsiaTest() : netstack_(netstack_ptr_.NewRequest()) {}

  ~NetworkChangeNotifierFuchsiaTest() override {}

  // Creates a NetworkChangeNotifier, which will be seeded with the list of
  // interfaces which have already been added to |netstack_|.
  void CreateNotifier() {
    notifier_.reset(new NetworkChangeNotifierFuchsia(std::move(netstack_ptr_)));
    NetworkChangeNotifier::AddNetworkChangeObserver(&observer_);
  }

  void TearDown() override {
    if (notifier_) {
      NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer_);
    }
  }

 protected:
  base::MessageLoopForIO message_loop_;
  testing::StrictMock<MockNetworkChangeObserver> observer_;
  fuchsia::netstack::NetstackPtr netstack_ptr_;
  FakeNetstack netstack_;

  // Allows us to allocate our own NetworkChangeNotifier for unit testing.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  std::unique_ptr<NetworkChangeNotifierFuchsia> notifier_;

  testing::InSequence seq_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierFuchsiaTest);
};

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChange) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushRouteTableEntry(CreateRouteTableEntry(kDefaultNic, true));

  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushRouteTableEntry(CreateRouteTableEntry(kDefaultNic, true));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChangeV6) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x01}),
                         CreateIPv6Address({0xfe, 0x80}), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x01}),
                         CreateIPv6Address({0xfe, 0x80}), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiInterfaceNoChange) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushInterface(
      CreateNetInterface(kSecondaryNic, fuchsia::netstack::NetInterfaceFlagUp,
                         0, CreateIPv4Address(169, 254, 0, 2),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushInterface(
      CreateNetInterface(kSecondaryNic, fuchsia::netstack::NetInterfaceFlagUp,
                         0, CreateIPv4Address(169, 254, 0, 3),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPNoChange) {
  std::vector<fuchsia::netstack::Subnet> addresses;
  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChange) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_UNKNOWN));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(10, 0, 0, 1), CreateIPv4Address(255, 255, 0, 0), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChangeV6) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x01}),
                         CreateIPv6Address({0xfe, 0x80}), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_UNKNOWN));
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x02}),
                         CreateIPv6Address({0xfe, 0x80}), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPChanged) {
  std::vector<fuchsia::netstack::Subnet> addresses;
  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_UNKNOWN));
  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x02}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(10, 0, 0, 1), CreateIPv4Address(255, 255, 0, 0),
      std::move(addresses)));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, Ipv6AdditionalIpChange) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_UNKNOWN));
  std::vector<fuchsia::netstack::Subnet> addresses;
  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDown) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  netstack_.PushInterface(
      CreateNetInterface(1, 0, 0, CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 0, 0), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceUp) {
  netstack_.PushInterface(
      CreateNetInterface(1, 0, 0, CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_UNKNOWN));
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 0, 0), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDeleted) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceAdded) {
  // Initial interface list is intentionally left empty.
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  EXPECT_CALL(observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_WIFI));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp,
      zircon::ethernet::INFO_FEATURE_WLAN, CreateIPv4Address(169, 254, 0, 1),
      CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, SecondaryInterfaceAddedNoop) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  netstack_.PushInterface(
      CreateNetInterface(kSecondaryNic, fuchsia::netstack::NetInterfaceFlagUp,
                         0, CreateIPv4Address(169, 254, 0, 2),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, SecondaryInterfaceDeletedNoop) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushInterface(
      CreateNetInterface(kSecondaryNic, fuchsia::netstack::NetInterfaceFlagUp,
                         0, CreateIPv4Address(169, 254, 0, 2),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  base::RunLoop().RunUntilIdle();

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.NotifyInterfaces();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, FoundWiFi) {
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp,
      zircon::ethernet::INFO_FEATURE_WLAN, CreateIPv4Address(169, 254, 0, 1),
      CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
            notifier_->GetCurrentConnectionType());
}

}  // namespace net

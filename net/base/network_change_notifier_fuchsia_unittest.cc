// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <fuchsia/hardware/ethernet/cpp/fidl.h>
#include <fuchsia/netstack/cpp/fidl_test_base.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const int kDefaultNic = 1;
const int kSecondaryNic = kDefaultNic + 1;

fuchsia::net::IpAddress CreateIPv6Address(std::vector<uint8_t> addr) {
  fuchsia::net::IpAddress output;
  for (size_t i = 0; i < addr.size(); ++i) {
    output.ipv6().addr[i] = addr[i];
  }
  return output;
}

fuchsia::net::Subnet CreateSubnet(const std::vector<uint8_t>& addr,
                                  uint8_t prefix) {
  fuchsia::net::Subnet output;
  output.addr = CreateIPv6Address(addr);
  output.prefix_len = prefix;
  return output;
}

fuchsia::net::IpAddress CreateIPv4Address(uint8_t a0,
                                          uint8_t a1,
                                          uint8_t a2,
                                          uint8_t a3) {
  fuchsia::net::IpAddress output;
  output.ipv4().addr[0] = a0;
  output.ipv4().addr[1] = a1;
  output.ipv4().addr[2] = a2;
  output.ipv4().addr[3] = a3;
  return output;
}

fuchsia::netstack::RouteTableEntry CreateRouteTableEntry(uint32_t nicid,
                                                         bool is_default) {
  fuchsia::netstack::RouteTableEntry output;
  output.nicid = nicid;

  if (is_default) {
    output.netmask = CreateIPv4Address(0, 0, 0, 0);
    output.destination = CreateIPv4Address(192, 168, 42, 0);
    output.gateway = CreateIPv4Address(192, 168, 42, 1);
  } else {
    output.netmask = CreateIPv4Address(255, 255, 255, 0);
    output.destination = CreateIPv4Address(192, 168, 43, 0);
    output.gateway = CreateIPv4Address(192, 168, 43, 1);
  }

  return output;
}

fuchsia::netstack::NetInterface CreateNetInterface(
    uint32_t id,
    uint32_t flags,
    uint32_t features,
    fuchsia::net::IpAddress address,
    fuchsia::net::IpAddress netmask,
    std::vector<fuchsia::net::Subnet> ipv6) {
  fuchsia::netstack::NetInterface output;
  output.name = "foo";
  output.id = id;
  output.flags = flags;
  output.features = features;
  output.addr = std::move(address);
  output.netmask = std::move(netmask);

  output.addr.Clone(&output.broadaddr);

  for (auto& x : ipv6) {
    output.ipv6addrs.push_back(std::move(x));
  }

  return output;
}

// Partial fake implementation of a Netstack.
class FakeNetstack : public fuchsia::netstack::testing::Netstack_TestBase {
 public:
  explicit FakeNetstack(
      fidl::InterfaceRequest<fuchsia::netstack::Netstack> netstack_request)
      : binding_(this) {
    CHECK_EQ(ZX_OK, binding_.Bind(std::move(netstack_request)));
  }
  ~FakeNetstack() override = default;

  // Adds |interface| to the interface query response list.
  void PushInterface(fuchsia::netstack::NetInterface interface) {
    interfaces_.push_back(std::move(interface));
  }

  // Sends the accumulated |interfaces_| to the OnInterfacesChanged event.
  void NotifyInterfaces() {
    binding_.events().OnInterfacesChanged(std::move(interfaces_));
    interfaces_.clear();
  }

  void SetOnGetRouteTableCallback(base::OnceClosure callback) {
    on_get_route_table_ = std::move(callback);
  }

 private:
  void GetInterfaces(GetInterfacesCallback callback) override {
    callback(std::move(interfaces_));
  }

  void GetRouteTable(GetRouteTableCallback callback) override {
    std::vector<fuchsia::netstack::RouteTableEntry> table(2);
    table[0] = CreateRouteTableEntry(kDefaultNic, true);
    table[1] = CreateRouteTableEntry(kSecondaryNic, false);
    callback(std::move(table));

    if (on_get_route_table_)
      std::move(on_get_route_table_).Run();
  }

  void NotImplemented_(const std::string& name) override {
    LOG(FATAL) << "Unimplemented function called: " << name;
  }

  std::vector<fuchsia::netstack::NetInterface> interfaces_;

  fidl::Binding<fuchsia::netstack::Netstack> binding_;

  base::OnceClosure on_get_route_table_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetstack);
};

class FakeNetstackAsync {
 public:
  explicit FakeNetstackAsync(
      fidl::InterfaceRequest<fuchsia::netstack::Netstack> netstack_request)
      : thread_("Netstack Thread") {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    CHECK(thread_.StartWithOptions(options));
    netstack_ = base::SequenceBound<FakeNetstack>(thread_.task_runner(),
                                                  std::move(netstack_request));
  }
  ~FakeNetstackAsync() = default;

  // Asynchronously update the state of the netstack.
  void PushInterface(fuchsia::netstack::NetInterface interface) {
    netstack_.Post(FROM_HERE, &FakeNetstack::PushInterface,
                   std::move(interface));
  }
  void NotifyInterfaces() {
    netstack_.Post(FROM_HERE, &FakeNetstack::NotifyInterfaces);
  }

  void SetOnGetRouteTableCallback(base::OnceClosure callback) {
    netstack_.Post(FROM_HERE, &FakeNetstack::SetOnGetRouteTableCallback,
                   std::move(callback));
  }

  // Ensure that any PushInterface() or NotifyInterfaces() have been processed.
  void FlushNetstackThread() {
    // Ensure that pending Push*() and Notify*() calls were processed.
    thread_.FlushForTesting();
  }

 private:
  base::Thread thread_;
  base::SequenceBound<FakeNetstack> netstack_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetstackAsync);
};

class MockConnectionTypeObserver
    : public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  MOCK_METHOD1(OnConnectionTypeChanged,
               void(NetworkChangeNotifier::ConnectionType));
};

class MockIPAddressObserver : public NetworkChangeNotifier::IPAddressObserver {
 public:
  MOCK_METHOD0(OnIPAddressChanged, void());
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
  ~NetworkChangeNotifierFuchsiaTest() override = default;

  // Creates a NetworkChangeNotifier and spins the MessageLoop to allow it to
  // populate from the list of interfaces which have already been added to
  // |netstack_|. |observer_| is registered last, so that tests need only
  // express expectations on changes they make themselves.
  void CreateNotifier(uint32_t required_features = 0) {
    // Ensure that the Netstack internal state is up-to-date before the
    // notifier queries it.
    netstack_.FlushNetstackThread();

    // Use a noop DNS notifier.
    dns_config_notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
        nullptr /* task_runner */, nullptr /* dns_config_service */);
    notifier_.reset(new NetworkChangeNotifierFuchsia(
        std::move(netstack_ptr_), required_features,
        dns_config_notifier_.get()));

    NetworkChangeNotifier::AddConnectionTypeObserver(&observer_);
    NetworkChangeNotifier::AddIPAddressObserver(&ip_observer_);
  }

  void TearDown() override {
    // Spin the loops to catch any unintended notifications.
    netstack_.FlushNetstackThread();
    base::RunLoop().RunUntilIdle();

    if (notifier_) {
      NetworkChangeNotifier::RemoveConnectionTypeObserver(&observer_);
      NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer_);
    }
  }

  // Causes FakeNetstack to emit NotifyInterfaces(), and then runs the loop
  // until the GetRouteTable() is called.
  void NetstackNotifyInterfacesAndWaitForGetRouteTable() {
    base::RunLoop loop;
    netstack_.SetOnGetRouteTableCallback(loop.QuitClosure());
    netstack_.NotifyInterfaces();
    loop.Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  testing::StrictMock<MockConnectionTypeObserver> observer_;
  testing::StrictMock<MockIPAddressObserver> ip_observer_;
  testing::StrictMock<MockNetworkChangeObserver> network_change_observer_;

  fuchsia::netstack::NetstackPtr netstack_ptr_;
  FakeNetstackAsync netstack_;

  // Allows us to allocate our own NetworkChangeNotifier for unit testing.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
  std::unique_ptr<NetworkChangeNotifierFuchsia> notifier_;

  testing::InSequence seq_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierFuchsiaTest);
};

TEST_F(NetworkChangeNotifierFuchsiaTest, InitialState) {
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NotifyNetworkChangeOnInitialIPChange) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp,
                         fuchsia::hardware::ethernet::INFO_FEATURE_WLAN,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  // Add and remove network_change_observer_ since it's only used in this method
  // gtest gives warnings on unused mocks if put into setup/teardown.
  NetworkChangeNotifier::AddNetworkChangeObserver(&network_change_observer_);
  EXPECT_CALL(network_change_observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
  EXPECT_CALL(network_change_observer_,
              OnNetworkChanged(NetworkChangeNotifier::CONNECTION_WIFI));
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());
  // Changing the IP address will now trigger network change as well since it is
  // currently out of sync
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp,
      fuchsia::hardware::ethernet::INFO_FEATURE_WLAN,
      CreateIPv4Address(10, 0, 0, 1), CreateIPv4Address(255, 255, 0, 0), {}));
  NetstackNotifyInterfacesAndWaitForGetRouteTable();

  NetworkChangeNotifier::RemoveNetworkChangeObserver(&network_change_observer_);
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChange) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));

  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.NotifyInterfaces();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChangeV6) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x01}),
                         CreateIPv6Address({0xfe, 0x80}), {}));
  CreateNotifier();

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x01}),
                         CreateIPv6Address({0xfe, 0x80}), {}));
  netstack_.NotifyInterfaces();
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

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushInterface(
      CreateNetInterface(kSecondaryNic, fuchsia::netstack::NetInterfaceFlagUp,
                         0, CreateIPv4Address(169, 254, 0, 3),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.NotifyInterfaces();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPNoChange) {
  std::vector<fuchsia::net::Subnet> addresses;
  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));

  CreateNotifier();

  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));
  netstack_.NotifyInterfaces();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChange) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();

  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(10, 0, 0, 1), CreateIPv4Address(255, 255, 0, 0), {}));

  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChangeV6) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x01}),
                         CreateIPv6Address({0xfe, 0x80}), {}));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv6Address({0xfe, 0x80, 0x02}),
                         CreateIPv6Address({0xfe, 0x80}), {}));

  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPChanged) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());

  std::vector<fuchsia::net::Subnet> addresses;
  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));

  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x02}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(10, 0, 0, 1), CreateIPv4Address(255, 255, 0, 0),
      std::move(addresses)));

  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, Ipv6AdditionalIpChange) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));

  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  std::vector<fuchsia::net::Subnet> addresses;
  addresses.push_back(CreateSubnet({0xfe, 0x80, 0x01}, 2));
  netstack_.PushInterface(CreateNetInterface(
      kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
      CreateIPv4Address(169, 254, 0, 1), CreateIPv4Address(255, 255, 255, 0),
      std::move(addresses)));

  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDown) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());
  EXPECT_CALL(observer_,
              OnConnectionTypeChanged(NetworkChangeNotifier::CONNECTION_NONE));

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));

  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  netstack_.PushInterface(
      CreateNetInterface(1, 0, 0, CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 0, 0), {}));

  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceUp) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());
  EXPECT_CALL(observer_, OnConnectionTypeChanged(
                             NetworkChangeNotifier::CONNECTION_UNKNOWN));

  netstack_.PushInterface(
      CreateNetInterface(1, 0, 0, CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));

  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 0, 0), {}));

  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDeleted) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());
  EXPECT_CALL(observer_,
              OnConnectionTypeChanged(NetworkChangeNotifier::CONNECTION_NONE));

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  // NotifyInterfaces() with no new PushInterfaces() means removing everything.
  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceAdded) {
  EXPECT_CALL(ip_observer_, OnIPAddressChanged());
  EXPECT_CALL(observer_,
              OnConnectionTypeChanged(NetworkChangeNotifier::CONNECTION_WIFI));

  // Initial interface list is intentionally left empty.
  CreateNotifier();

  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp,
                         fuchsia::hardware::ethernet::INFO_FEATURE_WLAN,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));

  NetstackNotifyInterfacesAndWaitForGetRouteTable();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, SecondaryInterfaceAddedNoop) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();

  netstack_.PushInterface(
      CreateNetInterface(kSecondaryNic, fuchsia::netstack::NetInterfaceFlagUp,
                         0, CreateIPv4Address(169, 254, 0, 2),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));

  netstack_.NotifyInterfaces();
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

  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));

  netstack_.NotifyInterfaces();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, FoundWiFi) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp,
                         fuchsia::hardware::ethernet::INFO_FEATURE_WLAN,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, FindsInterfaceWithRequiredFeature) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp,
                         fuchsia::hardware::ethernet::INFO_FEATURE_WLAN,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier(fuchsia::hardware::ethernet::INFO_FEATURE_WLAN);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IgnoresInterfaceWithMissingFeature) {
  netstack_.PushInterface(
      CreateNetInterface(kDefaultNic, fuchsia::netstack::NetInterfaceFlagUp, 0,
                         CreateIPv4Address(169, 254, 0, 1),
                         CreateIPv4Address(255, 255, 255, 0), {}));
  CreateNotifier(fuchsia::hardware::ethernet::INFO_FEATURE_WLAN);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());
}

}  // namespace net

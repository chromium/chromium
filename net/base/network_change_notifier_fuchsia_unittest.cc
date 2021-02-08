// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <fuchsia/hardware/ethernet/cpp/fidl.h>
#include <fuchsia/netstack/cpp/fidl_test_base.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
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

enum : uint32_t { kDefaultInterfaceId = 1, kSecondaryInterfaceId = 2 };

using IPv4Octets = std::array<uint8_t, 4>;
using IPv6Octets = std::array<uint8_t, 16>;

constexpr IPv4Octets kIPv4DefaultGatewayNetmask = {0, 0, 0, 0};
constexpr IPv4Octets kIPv4DefaultGatewayAddress = {192, 168, 0, 1};

constexpr IPv4Octets kDefaultIPv4Address = {192, 168, 0, 2};
constexpr IPv4Octets kDefaultIPv4Netmask = {255, 255, 0, 0};
constexpr IPv4Octets kSecondaryIPv4Address = {10, 0, 0, 1};
constexpr IPv4Octets kSecondaryIPv4Netmask = {255, 0, 0, 0};

constexpr IPv6Octets kDefaultIPv6Address = {0xfe, 0x80, 0x01};
constexpr IPv6Octets kDefaultIPv6Netmask = {0xfe, 0x80};
constexpr IPv6Octets kSecondaryIPv6Address = {0xfe, 0x80, 0x02};
constexpr IPv6Octets kSecondaryIPv6Netmask = {0xfe, 0x80};

fuchsia::net::IpAddress IpAddressFrom(IPv4Octets octets) {
  fuchsia::net::IpAddress output;
  output.ipv4().addr = octets;
  return output;
}

fuchsia::net::IpAddress IpAddressFrom(IPv6Octets octets) {
  fuchsia::net::IpAddress output;
  output.ipv6().addr = octets;
  return output;
}

fuchsia::net::Subnet SubnetFrom(IPv6Octets octets, uint8_t prefix) {
  fuchsia::net::Subnet output;
  output.addr = IpAddressFrom(octets);
  output.prefix_len = prefix;
  return output;
}

fuchsia::netstack::NetInterface DefaultNetInterface() {
  // For most tests a live interface with an IPv4 address and no |features| set
  // is sufficient.
  fuchsia::netstack::NetInterface interface;
  interface.id = kDefaultInterfaceId;
  interface.flags = fuchsia::netstack::Flags::UP;
  interface.features = {};
  interface.addr = IpAddressFrom(kDefaultIPv4Address);
  interface.netmask = IpAddressFrom(kDefaultIPv4Netmask);
  interface.broadaddr = IpAddressFrom(kDefaultIPv4Address);
  return interface;
}

fuchsia::netstack::NetInterface SecondaryNetInterface() {
  // For most tests a live interface with an IPv4 address and no |features| set
  // is sufficient.
  fuchsia::netstack::NetInterface interface;
  interface.id = kSecondaryInterfaceId;
  interface.flags = fuchsia::netstack::Flags::UP;
  interface.features = {};
  interface.addr = IpAddressFrom(kSecondaryIPv4Address);
  interface.netmask = IpAddressFrom(kSecondaryIPv4Netmask);
  interface.broadaddr = IpAddressFrom(kSecondaryIPv4Address);
  return interface;
}

std::vector<fuchsia::netstack::NetInterface> CloneNetInterfaces(
    const std::vector<fuchsia::netstack::NetInterface>& interfaces) {
  std::vector<fuchsia::netstack::NetInterface> interfaces_copy(
      interfaces.size());
  for (size_t i = 0; i < interfaces.size(); ++i) {
    CHECK_EQ(ZX_OK, interfaces[i].Clone(&interfaces_copy[i]));
  }
  return interfaces_copy;
}

// Partial fake implementation of a Netstack.
class FakeNetstack : public fuchsia::netstack::testing::Netstack_TestBase {
 public:
  FakeNetstack() = default;
  FakeNetstack(const FakeNetstack&) = delete;
  FakeNetstack& operator=(const FakeNetstack&) = delete;
  ~FakeNetstack() override = default;

  void Bind(
      fidl::InterfaceRequest<fuchsia::netstack::Netstack> netstack_request) {
    CHECK_EQ(ZX_OK, binding_.Bind(std::move(netstack_request)));
    SetInterfaces(std::move(interfaces_), std::move(second_interfaces_));
  }

  // Sets the interfaces reported by the fake Netstack and sends an
  // OnInterfacesChanged() event to the client.
  // If specified, |second_interfaces_| will be notified immediately after
  // |interfaces|, ensuring that no requests could be processed in-between the
  // two notifications.
  void SetInterfaces(
      std::vector<fuchsia::netstack::NetInterface> interfaces,
      base::Optional<std::vector<fuchsia::netstack::NetInterface>>
          second_interfaces = base::nullopt) {
    interfaces_ = std::move(interfaces);
    second_interfaces_ = std::move(second_interfaces);
    if (binding_.is_bound()) {
      binding_.events().OnInterfacesChanged(CloneNetInterfaces(interfaces_));
      if (second_interfaces_) {
        auto interfaces = std::move(second_interfaces_);
        SetInterfaces(std::move(interfaces.value()));
      }
    }
  }

 private:
  void GetRouteTable(GetRouteTableCallback callback) override {
    CHECK(binding_.is_bound());
    std::vector<fuchsia::netstack::RouteTableEntry> table(2);

    table[0].nicid = kDefaultInterfaceId;
    table[0].netmask = IpAddressFrom(kIPv4DefaultGatewayNetmask);
    table[0].destination = IpAddressFrom(kDefaultIPv4Address);
    table[0].gateway = IpAddressFrom(kIPv4DefaultGatewayAddress);

    table[1].nicid = kSecondaryInterfaceId;
    table[1].netmask = IpAddressFrom(kSecondaryIPv4Netmask);
    table[1].destination = IpAddressFrom(kSecondaryIPv4Address);
    table[1].gateway = IpAddressFrom(kSecondaryIPv4Address);

    callback(std::move(table));
  }

  void NotImplemented_(const std::string& name) override {
    LOG(FATAL) << "Unimplemented function called: " << name;
  }

  std::vector<fuchsia::netstack::NetInterface> interfaces_;
  base::Optional<std::vector<fuchsia::netstack::NetInterface>>
      second_interfaces_;

  fidl::Binding<fuchsia::netstack::Netstack> binding_{this};
};

class FakeNetstackAsync {
 public:
  FakeNetstackAsync() : thread_("Netstack Thread") {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    CHECK(thread_.StartWithOptions(options));
    netstack_ = base::SequenceBound<FakeNetstack>(thread_.task_runner());
  }
  FakeNetstackAsync(const FakeNetstackAsync&) = delete;
  FakeNetstackAsync& operator=(const FakeNetstackAsync&) = delete;
  ~FakeNetstackAsync() = default;

  void Bind(
      fidl::InterfaceRequest<fuchsia::netstack::Netstack> netstack_request) {
    netstack_.Post(FROM_HERE, &FakeNetstack::Bind, std::move(netstack_request));
  }

  // Asynchronously update the state of the netstack.
  void SetInterfaces(
      const std::vector<fuchsia::netstack::NetInterface>& interfaces) {
    netstack_.Post(FROM_HERE, &FakeNetstack::SetInterfaces,
                   CloneNetInterfaces(interfaces), base::nullopt);
  }

  // Asynchronously set both an initial state, and a second set of interfaces
  // to report an update for immediately after the first.
  void SetInterfacesAndImmediatelyReSet(
      const std::vector<fuchsia::netstack::NetInterface>& interfaces,
      const std::vector<fuchsia::netstack::NetInterface>& second_interfaces) {
    netstack_.Post(FROM_HERE, &FakeNetstack::SetInterfaces,
                   CloneNetInterfaces(interfaces),
                   CloneNetInterfaces(second_interfaces));
  }

  // Ensures that any SetInterfaces() or SendOnInterfacesChanged() calls have
  // been processed.
  void FlushNetstackThread() {
    thread_.FlushForTesting();
  }

 private:
  base::Thread thread_;
  base::SequenceBound<FakeNetstack> netstack_;
};

template <class T>
class ResultReceiver {
 public:
  ~ResultReceiver() { EXPECT_EQ(entries_.size(), 0u); }
  bool RunAndExpectEntries(std::vector<T> expected_entries) {
    if (entries_.size() < expected_entries.size()) {
      base::RunLoop loop;
      base::AutoReset<size_t> size(&expected_count_, expected_entries.size());
      base::AutoReset<base::OnceClosure> quit(&quit_loop_, loop.QuitClosure());
      loop.Run();
    }
    return expected_entries == std::exchange(entries_, {});
  }
  void AddEntry(T entry) {
    entries_.push_back(entry);
    if (quit_loop_ && entries_.size() >= expected_count_)
      std::move(quit_loop_).Run();
  }

 protected:
  size_t expected_count_ = 0u;
  std::vector<T> entries_;
  base::OnceClosure quit_loop_;
};

// Accumulates the list of ConnectionTypes notified via OnConnectionTypeChanged.
class FakeConnectionTypeObserver
    : public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  FakeConnectionTypeObserver() {
    NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }
  ~FakeConnectionTypeObserver() final {
    NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  }

  bool RunAndExpectConnectionTypes(
      std::vector<NetworkChangeNotifier::ConnectionType> sequence) {
    return receiver_.RunAndExpectEntries(sequence);
  }

  // ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) final {
    receiver_.AddEntry(type);
  }

 protected:
  ResultReceiver<NetworkChangeNotifier::ConnectionType> receiver_;
};

// Accumulates the list of ConnectionTypes notified via OnConnectionTypeChanged.
class FakeNetworkChangeObserver
    : public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  FakeNetworkChangeObserver() {
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }
  ~FakeNetworkChangeObserver() final {
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  bool RunAndExpectNetworkChanges(
      std::vector<NetworkChangeNotifier::ConnectionType> sequence) {
    return receiver_.RunAndExpectEntries(sequence);
  }

  // NetworkChangeObserver implementation.
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) final {
    receiver_.AddEntry(type);
  }

 protected:
  ResultReceiver<NetworkChangeNotifier::ConnectionType> receiver_;
};

// Accumulates the list of ConnectionTypes notified via OnConnectionTypeChanged.
class FakeIPAddressObserver : public NetworkChangeNotifier::IPAddressObserver {
 public:
  FakeIPAddressObserver() { NetworkChangeNotifier::AddIPAddressObserver(this); }
  ~FakeIPAddressObserver() final {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
    EXPECT_EQ(ip_change_count_, 0u);
  }

  bool RunAndExpectCallCount(size_t expected_count) {
    if (ip_change_count_ < expected_count) {
      base::RunLoop loop;
      base::AutoReset<size_t> expectation(&expected_count_, expected_count);
      base::AutoReset<base::OnceClosure> quit(&quit_loop_, loop.QuitClosure());
      loop.Run();
    }
    return std::exchange(ip_change_count_, 0u) == expected_count;
  }

  // IPAddressObserver implementation.
  void OnIPAddressChanged() final {
    ip_change_count_++;
    if (quit_loop_ && ip_change_count_ >= expected_count_)
      std::move(quit_loop_).Run();
  }

 protected:
  size_t expected_count_ = 0u;
  size_t ip_change_count_ = 0u;
  base::OnceClosure quit_loop_;
};

}  // namespace

class NetworkChangeNotifierFuchsiaTest : public testing::Test {
 public:
  NetworkChangeNotifierFuchsiaTest() = default;
  NetworkChangeNotifierFuchsiaTest(const NetworkChangeNotifierFuchsiaTest&) =
      delete;
  NetworkChangeNotifierFuchsiaTest& operator=(
      const NetworkChangeNotifierFuchsiaTest&) = delete;
  ~NetworkChangeNotifierFuchsiaTest() override = default;

  // Creates a NetworkChangeNotifier and spins the MessageLoop to allow it to
  // populate from the list of interfaces which have already been added to
  // |netstack_|. |observer_| is registered last, so that tests need only
  // express expectations on changes they make themselves.
  void CreateNotifier(
      fuchsia::hardware::ethernet::Features required_features = {}) {
    // Ensure that the Netstack internal state is up-to-date before the
    // notifier queries it.
    netstack_.FlushNetstackThread();

    CHECK(!netstack_handle_);
    netstack_.Bind(netstack_handle_.NewRequest());

    // Use a noop DNS notifier.
    dns_config_notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
        nullptr /* task_runner */, nullptr /* dns_config_service */);
    notifier_.reset(new NetworkChangeNotifierFuchsia(
        std::move(netstack_handle_), required_features,
        dns_config_notifier_.get()));

    type_observer_ = std::make_unique<FakeConnectionTypeObserver>();
    ip_observer_ = std::make_unique<FakeIPAddressObserver>();
  }

  void TearDown() override {
    // Spin the loops to catch any unintended notifications.
    netstack_.FlushNetstackThread();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  fidl::InterfaceHandle<fuchsia::netstack::Netstack> netstack_handle_;
  FakeNetstackAsync netstack_;

  // Allows us to allocate our own NetworkChangeNotifier for unit testing.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
  std::unique_ptr<NetworkChangeNotifierFuchsia> notifier_;

  std::unique_ptr<FakeConnectionTypeObserver> type_observer_;
  std::unique_ptr<FakeIPAddressObserver> ip_observer_;
};

TEST_F(NetworkChangeNotifierFuchsiaTest, InitialState) {
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfacesChangeDuringConstruction) {
  // Set a live interface with an IP address and create the notifier.
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].features = fuchsia::hardware::ethernet::Features::WLAN;

  // Trigger two notifications, to verify that the notifier copes with receiving
  // notifications while waiting on the initial GetRouteTable() returning.
  netstack_.SetInterfacesAndImmediatelyReSet(interfaces, interfaces);

  CreateNotifier();
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NotifyNetworkChangeOnInitialIPChange) {
  // Set a live interface with an IP address and create the notifier.
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].features = fuchsia::hardware::ethernet::Features::WLAN;

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();

  // Add the NetworkChangeNotifier, and change the IP address. This should
  // trigger a network change notification, since the IP address is out-of-sync.
  FakeNetworkChangeObserver network_change_observer;

  interfaces[0].addr = IpAddressFrom(kSecondaryIPv4Address);
  netstack_.SetInterfaces(interfaces);

  EXPECT_TRUE(network_change_observer.RunAndExpectNetworkChanges(
      {NetworkChangeNotifier::CONNECTION_NONE,
       NetworkChangeNotifier::CONNECTION_WIFI}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChange) {
  // Set a live interface with an IP address and create the notifier.
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  // Leave the set of interfaces unchanged, but re-send OnInterfacesChanged.
  netstack_.SetInterfaces(interfaces);
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChangeV6) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].addr = IpAddressFrom(kDefaultIPv6Address);
  interfaces[0].netmask = IpAddressFrom(kDefaultIPv6Netmask);

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();

  // Leave the set of interfaces unchanged, but re-send OnInterfacesChanged.
  netstack_.SetInterfaces(interfaces);
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiInterfaceNoChange) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(2);
  interfaces[0] = DefaultNetInterface();
  interfaces[1] = SecondaryNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();

  // Leave the set of interfaces unchanged, but re-send OnInterfacesChanged.
  netstack_.SetInterfaces(interfaces);
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPNoChange) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].ipv6addrs.push_back(SubnetFrom(kDefaultIPv6Address, 2));

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();

  // Leave the set of interfaces unchanged, but re-send OnInterfacesChanged.
  netstack_.SetInterfaces(interfaces);
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChange) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  interfaces[0].addr = IpAddressFrom(kSecondaryIPv4Address);
  netstack_.SetInterfaces(interfaces);

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChangeV6) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].addr = IpAddressFrom(kDefaultIPv6Address);
  interfaces[0].netmask = IpAddressFrom(kDefaultIPv6Netmask);
  interfaces[0].broadaddr = IpAddressFrom(kDefaultIPv6Address);

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  interfaces[0].addr = IpAddressFrom(kSecondaryIPv6Address);
  interfaces[0].netmask = IpAddressFrom(kSecondaryIPv6Netmask);
  interfaces[0].broadaddr = IpAddressFrom(kSecondaryIPv6Address);
  netstack_.SetInterfaces(interfaces);

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPChanged) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].ipv6addrs.push_back(SubnetFrom(kDefaultIPv6Address, 2));

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  interfaces[0].addr = IpAddressFrom(kSecondaryIPv4Address);
  interfaces[0].netmask = IpAddressFrom(kSecondaryIPv4Netmask);
  interfaces[0].broadaddr = IpAddressFrom(kSecondaryIPv4Address);
  interfaces[0].ipv6addrs[0] = SubnetFrom(kSecondaryIPv6Address, 2);
  netstack_.SetInterfaces(interfaces);

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, Ipv6AdditionalIpChange) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  interfaces[0].ipv6addrs.push_back(SubnetFrom(kDefaultIPv6Address, 2));
  netstack_.SetInterfaces(interfaces);

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDown) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  interfaces[0].flags = {};
  netstack_.SetInterfaces(interfaces);

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_NONE}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceUp) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].flags = {};

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());

  interfaces[0].flags = fuchsia::netstack::Flags::UP;
  netstack_.SetInterfaces(interfaces);

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDeleted) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  netstack_.SetInterfaces({});

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_NONE}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceAdded) {
  // Initial interface list is intentionally left empty.
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());

  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].features = fuchsia::hardware::ethernet::Features::WLAN;

  netstack_.SetInterfaces(interfaces);

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, SecondaryInterfaceAddedNoop) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();

  interfaces.push_back(SecondaryNetInterface());
  netstack_.SetInterfaces(interfaces);
}

TEST_F(NetworkChangeNotifierFuchsiaTest, SecondaryInterfaceDeletedNoop) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(2);
  interfaces[0] = DefaultNetInterface();
  interfaces[1] = SecondaryNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();

  interfaces.pop_back();
  netstack_.SetInterfaces(interfaces);
}

TEST_F(NetworkChangeNotifierFuchsiaTest, FoundWiFi) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].features = fuchsia::hardware::ethernet::Features::WLAN;

  netstack_.SetInterfaces(interfaces);
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, FindsInterfaceWithRequiredFeature) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();
  interfaces[0].features = fuchsia::hardware::ethernet::Features::WLAN;

  netstack_.SetInterfaces(interfaces);
  CreateNotifier(fuchsia::hardware::ethernet::Features::WLAN);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IgnoresInterfaceWithMissingFeature) {
  std::vector<fuchsia::netstack::NetInterface> interfaces(1);
  interfaces[0] = DefaultNetInterface();

  netstack_.SetInterfaces(interfaces);
  CreateNotifier(fuchsia::hardware::ethernet::Features::WLAN);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());
}

}  // namespace net

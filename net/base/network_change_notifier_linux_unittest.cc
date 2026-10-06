// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <sys/socket.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/callback_helpers.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "build/config/linux/dbus/buildflags.h"
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/cronet_buildflags.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(CRONET_BUILD)
#include <dbus/dbus.h>

#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal_constants.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#endif

namespace net {

namespace {

#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(CRONET_BUILD)
using ::testing::_;

constexpr char kNetworkMonitorInterface[] =
    "org.freedesktop.portal.NetworkMonitor";

class TestConnectionCostObserver
    : public NetworkChangeNotifier::ConnectionCostObserver {
 public:
  void OnConnectionCostChanged(
      NetworkChangeNotifier::ConnectionCost cost) override {
    last_cost_ = cost;
    ++cost_changed_calls_;
  }

  int cost_changed_calls() const { return cost_changed_calls_; }
  NetworkChangeNotifier::ConnectionCost last_cost() const { return last_cost_; }

 private:
  int cost_changed_calls_ = 0;
  NetworkChangeNotifier::ConnectionCost last_cost_ =
      NetworkChangeNotifier::CONNECTION_COST_UNKNOWN;
};
#endif

}  // namespace

class NetworkChangeNotifierLinuxTest : public testing::Test {
 public:
  NetworkChangeNotifierLinuxTest() = default;
  NetworkChangeNotifierLinuxTest(const NetworkChangeNotifierLinuxTest&) =
      delete;
  NetworkChangeNotifierLinuxTest& operator=(
      const NetworkChangeNotifierLinuxTest&) = delete;
  ~NetworkChangeNotifierLinuxTest() override {
#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(CRONET_BUILD)
    notifier_.reset();
    dns_config_notifier_.reset();
    mock_proxy_.reset();
    mock_bus_.reset();
#endif
  }

  base::ScopedFD CreateDummyNetlinkSocket() {
    int fds[2];
    EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    netlink_peer_fd_.reset(fds[1]);
    return base::ScopedFD(fds[0]);
  }

  void CreateNotifier() {
    // Use a noop DNS notifier and a dummy netlink socket so the test remains
    // hermetic and does not connect to the host netlink or session D-Bus.
    dns_config_notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
        nullptr /* task_runner */, nullptr /* dns_config_service */);
    notifier_ = NetworkChangeNotifierLinux::CreateWithSocketForTesting(
        absl::flat_hash_set<std::string>(), CreateDummyNetlinkSocket());
  }

#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(CRONET_BUILD)
  void InitPortalMockBus() {
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), dbus_xdg::kPortalServiceName,
        dbus::ObjectPath(dbus_xdg::kPortalObjectPath));

    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).WillRepeatedly([]() {});
    // Provide a real task runner and verify ShutdownAndBlock is never called
    // on an injected (non-owned) bus.
    EXPECT_CALL(*mock_bus_, GetDBusTaskRunner())
        .WillRepeatedly(
            testing::Return(task_environment_.GetMainThreadTaskRunner().get()));
    EXPECT_CALL(*mock_bus_, ShutdownAndBlock()).Times(0);
    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(dbus_xdg::kPortalServiceName,
                               dbus::ObjectPath(dbus_xdg::kPortalObjectPath)))
        .WillRepeatedly(testing::Return(mock_proxy_.get()));
    EXPECT_CALL(*mock_proxy_, SetNameOwnerChangedCallback(_))
        .WillOnce([&](dbus::ObjectProxy::NameOwnerChangedCallback cb) {
          name_owner_changed_callback_ = std::move(cb);
        });
    EXPECT_CALL(*mock_proxy_,
                ConnectToSignal(kNetworkMonitorInterface, "changed", _, _))
        .WillOnce([&](const std::string& interface_name,
                      const std::string& signal_name,
                      dbus::ObjectProxy::SignalCallback signal_cb,
                      dbus::ObjectProxy::OnConnectedCallback on_connected_cb) {
          signal_callback_ = std::move(signal_cb);
          std::move(on_connected_cb).Run(interface_name, signal_name, true);
        });
  }

  void CreateNotifierWithBus(scoped_refptr<dbus::Bus> bus) {
    dns_config_notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
        nullptr /* task_runner */, nullptr /* dns_config_service */);
    notifier_ = NetworkChangeNotifierLinux::CreateWithBusForTesting(bus.get());
  }
#endif

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;

  // Allows us to allocate our own NetworkChangeNotifier for unit testing.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  base::ScopedFD netlink_peer_fd_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
  std::unique_ptr<NetworkChangeNotifierLinux> notifier_;
#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(CRONET_BUILD)
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  dbus::ObjectProxy::SignalCallback signal_callback_;
  dbus::ObjectProxy::NameOwnerChangedCallback name_owner_changed_callback_;
#endif
};

// https://crbug.com/1441671
TEST_F(NetworkChangeNotifierLinuxTest, AddressTrackerLinuxSetDiffCallback) {
  CreateNotifier();
  AddressMapOwnerLinux* address_map_owner = notifier_->GetAddressMapOwner();
  ASSERT_TRUE(address_map_owner);
  internal::AddressTrackerLinux* address_tracker_linux =
      address_map_owner->GetAddressTrackerLinux();
  ASSERT_TRUE(address_tracker_linux);
  address_tracker_linux->GetInitialDataAndStartRecordingDiffs();
  address_tracker_linux->SetDiffCallback(base::DoNothing());
}

#if BUILDFLAG(USE_DBUS) && !BUILDFLAG(CRONET_BUILD)
TEST_F(NetworkChangeNotifierLinuxTest, PortalNetworkMonitorV3StatusAndChanged) {
  InitPortalMockBus();

  bool status_metered = true;

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
            method_call->GetMember() == "Get") {
          writer.AppendVariantOfUint32(3);
        } else if (method_call->GetInterface() == kNetworkMonitorInterface &&
                   method_call->GetMember() == "GetStatus") {
          std::map<std::string, dbus_utils::Variant> dict;
          dict["available"] = dbus_utils::Variant::Wrap<"b">(true);
          dict["metered"] = dbus_utils::Variant::Wrap<"b">(status_metered);
          dbus_utils::WriteValue(writer, dict);
        }
        std::move(callback).Run(response.get(), nullptr);
      });

  CreateNotifierWithBus(mock_bus_);
  EXPECT_TRUE(base::test::RunUntil([]() {
    return NetworkChangeNotifier::GetConnectionCost() ==
           NetworkChangeNotifier::CONNECTION_COST_METERED;
  }));
  // The portal's `available` state does not affect the connection type, which
  // comes only from netlink (uninitialized here, so CONNECTION_NONE).
  EXPECT_EQ(NetworkChangeNotifier::GetConnectionType(),
            NetworkChangeNotifier::CONNECTION_NONE);

  TestConnectionCostObserver cost_observer;
  NetworkChangeNotifier::AddConnectionCostObserver(&cost_observer);

  // Transition to unmetered via the "changed" signal.
  status_metered = false;
  dbus::Signal changed_signal(kNetworkMonitorInterface, "changed");
  ASSERT_TRUE(signal_callback_);
  signal_callback_.Run(&changed_signal);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return cost_observer.cost_changed_calls() == 1; }));

  EXPECT_EQ(NetworkChangeNotifier::GetConnectionCost(),
            NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  EXPECT_EQ(cost_observer.last_cost(),
            NetworkChangeNotifier::CONNECTION_COST_UNMETERED);

  NetworkChangeNotifier::RemoveConnectionCostObserver(&cost_observer);
}

TEST_F(NetworkChangeNotifierLinuxTest, PortalNetworkMonitorV2Fallback) {
  InitPortalMockBus();

  bool status_metered = true;

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
            method_call->GetMember() == "Get") {
          writer.AppendVariantOfUint32(2);
        } else if (method_call->GetInterface() == kNetworkMonitorInterface &&
                   method_call->GetMember() == "GetMetered") {
          writer.AppendBool(status_metered);
        }
        std::move(callback).Run(response.get(), nullptr);
      });

  CreateNotifierWithBus(mock_bus_);
  EXPECT_TRUE(base::test::RunUntil([]() {
    return NetworkChangeNotifier::GetConnectionCost() ==
           NetworkChangeNotifier::CONNECTION_COST_METERED;
  }));

  status_metered = false;
  dbus::Signal changed_signal(kNetworkMonitorInterface, "changed");
  ASSERT_TRUE(signal_callback_);
  signal_callback_.Run(&changed_signal);
  EXPECT_TRUE(base::test::RunUntil([]() {
    return NetworkChangeNotifier::GetConnectionCost() ==
           NetworkChangeNotifier::CONNECTION_COST_UNMETERED;
  }));
}

TEST_F(NetworkChangeNotifierLinuxTest,
       PortalNetworkMonitorLateStartAndOwnerReset) {
  InitPortalMockBus();

  bool portal_running = false;
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        if (!portal_running) {
          std::move(callback).Run(nullptr, nullptr);
          return;
        }
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
            method_call->GetMember() == "Get") {
          writer.AppendVariantOfUint32(3);
        } else if (method_call->GetInterface() == kNetworkMonitorInterface &&
                   method_call->GetMember() == "GetStatus") {
          std::map<std::string, dbus_utils::Variant> dict;
          dict["metered"] = dbus_utils::Variant::Wrap<"b">(true);
          dbus_utils::WriteValue(writer, dict);
        }
        std::move(callback).Run(response.get(), nullptr);
      });

  CreateNotifierWithBus(mock_bus_);
  // Initially the portal is not running, so the cost is not metered.
  EXPECT_NE(NetworkChangeNotifier::GetConnectionCost(),
            NetworkChangeNotifier::CONNECTION_COST_METERED);

  // Portal starts up and acquires bus ownership; monitor should query version
  // and status and transition to CONNECTION_COST_METERED.
  portal_running = true;
  ASSERT_TRUE(name_owner_changed_callback_);
  name_owner_changed_callback_.Run("", ":1.42");
  EXPECT_TRUE(base::test::RunUntil([]() {
    return NetworkChangeNotifier::GetConnectionCost() ==
           NetworkChangeNotifier::CONNECTION_COST_METERED;
  }));

  // Simulate the portal service losing its bus owner; cost should reset back
  // to the fallback cost.
  name_owner_changed_callback_.Run(":1.42", "");
  EXPECT_TRUE(base::test::RunUntil([]() {
    return NetworkChangeNotifier::GetConnectionCost() !=
           NetworkChangeNotifier::CONNECTION_COST_METERED;
  }));
}
#endif

}  // namespace net

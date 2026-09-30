// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux_portal.h"

#include <dbus/dbus.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal_constants.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace net {

namespace {

constexpr char kNetworkMonitorInterface[] =
    "org.freedesktop.portal.NetworkMonitor";
constexpr char kPropertyVersion[] = "version";
constexpr char kSignalChanged[] = "changed";
constexpr char kMethodGetStatus[] = "GetStatus";
constexpr char kMethodGetMetered[] = "GetMetered";
constexpr char kMethodGetAvailable[] = "GetAvailable";

class PortalMonitorImpl : public NetworkChangeNotifierLinux::PortalMonitor {
 public:
  PortalMonitorImpl(scoped_refptr<dbus::Bus> bus,
                    OnlineStateCallback online_state_callback,
                    ConnectionCostCallback connection_cost_callback)
      : bus_(std::move(bus)),
        owns_bus_(!bus_),
        online_state_callback_(std::move(online_state_callback)),
        connection_cost_callback_(std::move(connection_cost_callback)) {
    if (owns_bus_) {
      // Create a private session bus owned by this monitor rather than using
      // the process-global `dbus_thread_linux::GetSharedSessionBus()` or
      // `dbus_xdg::RequestXdgDesktopPortal()` singletons:
      // 1. In production, `NetworkChangeNotifierLinux` is a singleton created
      //    only once in the browser process (`BrowserMainLoop`), while the
      //    out-of-process network service uses `NetworkChangeNotifierPassive`.
      // 2. In unit test suites (e.g. `net_unittests`), cross-platform tests
      //    call `NetworkChangeNotifier::CreateIfNeeded()` across sequential
      //    `TaskEnvironment`s without calling
      //    `dbus_thread_linux::ShutdownOnDBusThreadAndBlock()`. Using
      //    process-global D-Bus singletons retains a stale origin `TaskRunner`
      //    from the first test's destroyed `TaskEnvironment`. Owning a private
      //    bus here and shutting it down in `~PortalMonitorImpl()` ties the
      //    bus lifetime to the notifier instance.
      // 3. `org.freedesktop.portal.NetworkMonitor` is a read-only status/signal
      //    interface that does not create `org.freedesktop.portal.Request`
      //    sessions or require `org.freedesktop.host.portal.Registry` app ID
      //    registration.
      dbus::Bus::Options options;
      options.bus_type = dbus::Bus::SESSION;
      options.connection_type = dbus::Bus::PRIVATE;
      options.dbus_task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
      bus_ = base::MakeRefCounted<dbus::Bus>(std::move(options));
    } else {
      bus_->AssertOnOriginThread();
    }
    portal_proxy_ =
        bus_->GetObjectProxy(dbus_xdg::kPortalServiceName,
                             dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
    portal_proxy_->SetNameOwnerChangedCallback(
        base::BindRepeating(&PortalMonitorImpl::OnNameOwnerChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    // Subscribe to the "changed" signal unconditionally in the constructor so
    // `dbus::ObjectProxy` installs the `NameOwnerChanged` match rule even if
    // the portal service is not yet running when `QueryVersion()` is called.
    portal_proxy_->ConnectToSignal(
        kNetworkMonitorInterface, kSignalChanged,
        base::BindRepeating(&PortalMonitorImpl::OnPortalChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&PortalMonitorImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    QueryVersion();
  }

  ~PortalMonitorImpl() override {
    portal_proxy_ = nullptr;
    if (owns_bus_ && bus_) {
      if (auto* task_runner = bus_->GetDBusTaskRunner()) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&dbus::Bus::ShutdownAndBlock, std::move(bus_)));
      }
    }
  }

 private:
  using PortalOnlineState = NetworkChangeNotifierLinux::PortalOnlineState;

  void QueryVersion() {
    dbus_utils::CallMethod<"ss", "v">(
        portal_proxy_, DBUS_INTERFACE_PROPERTIES, "Get",
        base::BindOnce(&PortalMonitorImpl::OnGetVersion,
                       weak_ptr_factory_.GetWeakPtr(), ++query_generation_),
        kNetworkMonitorInterface, kPropertyVersion);
  }

  void OnNameOwnerChanged(const std::string& /*old_owner*/,
                          const std::string& new_owner) {
    ++query_generation_;
    version_ = 0;
    online_state_callback_.Run(PortalOnlineState::kUnknown);
    connection_cost_callback_.Run(
        NetworkChangeNotifier::CONNECTION_COST_UNKNOWN);
    if (!new_owner.empty()) {
      QueryVersion();
    }
  }

  void OnGetVersion(uint64_t generation,
                    dbus_utils::CallMethodResultSig<"v"> result) {
    if (generation != query_generation_ || !result.has_value()) {
      return;
    }
    std::optional<uint32_t> version =
        std::move(std::get<0>(*result)).Take<uint32_t>();
    // Portal v1 exposed D-Bus properties instead of methods and is deprecated;
    // require v2+ (GetAvailable/GetMetered) or v3+ (GetStatus).
    if (!version || *version < 2) {
      return;
    }
    version_ = *version;
    QueryPortalStatus();
  }

  void OnSignalConnected(const std::string& /*interface_name*/,
                         const std::string& /*signal_name*/,
                         bool success) {
    if (success && version_ >= 2) {
      QueryPortalStatus();
    }
  }

  void OnPortalChanged(dbus::Signal* /*signal*/) { QueryPortalStatus(); }

  void QueryPortalStatus() {
    if (!portal_proxy_ || version_ < 2) {
      return;
    }
    const uint64_t generation = ++query_generation_;
    if (version_ >= 3) {
      dbus_utils::CallMethod<"", "a{sv}">(
          portal_proxy_, kNetworkMonitorInterface, kMethodGetStatus,
          base::BindOnce(&PortalMonitorImpl::OnGetStatus,
                         weak_ptr_factory_.GetWeakPtr(), generation));
    } else if (version_ == 2) {
      dbus_utils::CallMethod<"", "b">(
          portal_proxy_, kNetworkMonitorInterface, kMethodGetMetered,
          base::BindOnce(&PortalMonitorImpl::OnGetMetered,
                         weak_ptr_factory_.GetWeakPtr(), generation));
      dbus_utils::CallMethod<"", "b">(
          portal_proxy_, kNetworkMonitorInterface, kMethodGetAvailable,
          base::BindOnce(&PortalMonitorImpl::OnGetAvailable,
                         weak_ptr_factory_.GetWeakPtr(), generation));
    }
  }

  void OnGetStatus(uint64_t generation,
                   dbus_utils::CallMethodResultSig<"a{sv}"> result) {
    if (generation != query_generation_ || !result.has_value()) {
      return;
    }
    auto& status = std::get<0>(*result);

    // Use `available` rather than `connectivity` to determine online state so
    // that limited (2) and captive-portal (3) networks are still treated as
    // online when a default route is available.
    if (auto it = status.find("available"); it != status.end()) {
      if (std::optional<bool> available = std::move(it->second).Take<bool>()) {
        online_state_callback_.Run(*available ? PortalOnlineState::kOnline
                                              : PortalOnlineState::kOffline);
      }
    }

    if (auto it = status.find("metered"); it != status.end()) {
      if (std::optional<bool> metered = std::move(it->second).Take<bool>()) {
        UpdateConnectionCost(*metered);
      }
    }
  }

  void OnGetMetered(uint64_t generation,
                    dbus_utils::CallMethodResultSig<"b"> result) {
    if (generation != query_generation_ || !result.has_value()) {
      return;
    }
    UpdateConnectionCost(std::get<0>(*result));
  }

  void OnGetAvailable(uint64_t generation,
                      dbus_utils::CallMethodResultSig<"b"> result) {
    if (generation != query_generation_ || !result.has_value()) {
      return;
    }
    online_state_callback_.Run(std::get<0>(*result)
                                   ? PortalOnlineState::kOnline
                                   : PortalOnlineState::kOffline);
  }

  void UpdateConnectionCost(bool metered) {
    connection_cost_callback_.Run(
        metered ? NetworkChangeNotifier::CONNECTION_COST_METERED
                : NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  }

  scoped_refptr<dbus::Bus> bus_;
  const bool owns_bus_;
  raw_ptr<dbus::ObjectProxy> portal_proxy_ = nullptr;
  OnlineStateCallback online_state_callback_;
  ConnectionCostCallback connection_cost_callback_;
  uint32_t version_ = 0;
  uint64_t query_generation_ = 0;
  base::WeakPtrFactory<PortalMonitorImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<NetworkChangeNotifierLinux::PortalMonitor>
NetworkChangeNotifierLinux::PortalMonitor::Create(
    dbus::Bus* bus,
    OnlineStateCallback online_state_callback,
    ConnectionCostCallback connection_cost_callback) {
  if (!base::SequencedTaskRunner::HasCurrentDefault()) {
    return nullptr;
  }
  return std::make_unique<PortalMonitorImpl>(
      scoped_refptr<dbus::Bus>(bus), std::move(online_state_callback),
      std::move(connection_cost_callback));
}

}  // namespace net

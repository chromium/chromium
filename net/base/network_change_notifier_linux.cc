// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier_linux_portal.h"
#include "net/dns/dns_config_service_posix.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace net {

// A collection of objects that live on blocking threads.
class NetworkChangeNotifierLinux::BlockingThreadObjects {
 public:
  BlockingThreadObjects(
      const absl::flat_hash_set<std::string>& ignored_interfaces,
      scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner,
      bool netlink_initialized);
  BlockingThreadObjects(const BlockingThreadObjects&) = delete;
  BlockingThreadObjects& operator=(const BlockingThreadObjects&) = delete;

  // Plumbing for NetworkChangeNotifier::GetCurrentConnectionType.
  // Safe to call from any thread.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() {
    NetworkChangeNotifier::ConnectionType type =
        netlink_initialized_.load(std::memory_order_acquire)
            ? address_tracker_.GetCurrentConnectionType()
            : NetworkChangeNotifier::CONNECTION_NONE;
    if (!portal_online_state_enabled_) {
      return type;
    }
    PortalOnlineState portal_state =
        portal_online_state_.load(std::memory_order_relaxed);
    if (portal_state == PortalOnlineState::kOffline) {
      return NetworkChangeNotifier::CONNECTION_NONE;
    }
    if (type == NetworkChangeNotifier::CONNECTION_NONE &&
        portal_state == PortalOnlineState::kOnline) {
      return NetworkChangeNotifier::CONNECTION_UNKNOWN;
    }
    return type;
  }

  // Called on the sequence that created the owning
  // `NetworkChangeNotifierLinux` (the D-Bus origin thread, where
  // `PortalMonitor` callbacks run), not on `blocking_thread_runner_`. The
  // atomic `portal_online_state_` makes this safe; the connection type
  // re-evaluation is posted to `blocking_thread_runner_`.
  void SetPortalOnlineState(PortalOnlineState state) {
    if (portal_online_state_.exchange(state, std::memory_order_acq_rel) !=
        state) {
      blocking_thread_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &NetworkChangeNotifierLinux::BlockingThreadObjects::OnLinkChanged,
              weak_ptr_));
    }
  }

  void SetNetlinkInitializedForTesting() {
    netlink_initialized_.store(true, std::memory_order_release);
  }

  internal::AddressTrackerLinux* address_tracker() { return &address_tracker_; }

  // Begin watching for netlink changes.
  void Init();

  void InitForTesting(base::ScopedFD netlink_fd);  // IN-TEST

 private:
  void OnIPAddressChanged(IPAddressChangeType change_type);
  void OnLinkChanged();

  const bool portal_online_state_enabled_ = base::FeatureList::IsEnabled(
      features::kNetworkChangeNotifierPortalOnlineState);
  scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner_;
  // Used to detect online/offline state and IP address changes.
  internal::AddressTrackerLinux address_tracker_;
  std::atomic<bool> netlink_initialized_;
  std::atomic<PortalOnlineState> portal_online_state_{
      PortalOnlineState::kUnknown};
  NetworkChangeNotifier::ConnectionType last_type_ =
      NetworkChangeNotifier::CONNECTION_NONE;
  // Created in the constructor and bound/dereferenced/invalidated exclusively
  // on `blocking_thread_runner_` (where `BlockingThreadObjects` is destroyed
  // via `OnTaskRunnerDeleter`), avoiding `base::Unretained`.
  base::WeakPtr<BlockingThreadObjects> weak_ptr_;
  base::WeakPtrFactory<BlockingThreadObjects> weak_ptr_factory_{this};
};

NetworkChangeNotifierLinux::BlockingThreadObjects::BlockingThreadObjects(
    const absl::flat_hash_set<std::string>& ignored_interfaces,
    scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner,
    bool netlink_initialized)
    : blocking_thread_runner_(blocking_thread_runner),
      address_tracker_(
          base::BindRepeating(&NetworkChangeNotifierLinux::
                                  BlockingThreadObjects::OnIPAddressChanged,
                              base::Unretained(this)),
          base::BindRepeating(
              &NetworkChangeNotifierLinux::BlockingThreadObjects::OnLinkChanged,
              base::Unretained(this)),
          base::DoNothing(),
          ignored_interfaces,
          std::move(blocking_thread_runner)),
      netlink_initialized_(netlink_initialized) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

void NetworkChangeNotifierLinux::BlockingThreadObjects::Init() {
  address_tracker_.Init();
  last_type_ = GetCurrentConnectionType();
}

void NetworkChangeNotifierLinux::BlockingThreadObjects::InitForTesting(
    base::ScopedFD netlink_fd) {
  address_tracker_.InitWithFdForTesting(std::move(netlink_fd));  // IN-TEST
  last_type_ = GetCurrentConnectionType();
}

void NetworkChangeNotifierLinux::BlockingThreadObjects::OnIPAddressChanged(
    IPAddressChangeType change_type) {
  NetworkChangeNotifier::NotifyObserversOfIPAddressChange(change_type);
  // When the IP address of a network interface is added/deleted, the
  // connection type may have changed.
  OnLinkChanged();
}

void NetworkChangeNotifierLinux::BlockingThreadObjects::OnLinkChanged() {
  if (last_type_ != GetCurrentConnectionType()) {
    NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
    last_type_ = GetCurrentConnectionType();
    double max_bandwidth_mbps =
        NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype(
            last_type_ == CONNECTION_NONE ? SUBTYPE_NONE : SUBTYPE_UNKNOWN);
    NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChange(
        max_bandwidth_mbps, last_type_);
  }
}

// static
std::unique_ptr<NetworkChangeNotifierLinux>
NetworkChangeNotifierLinux::CreateWithSocketForTesting(
    const absl::flat_hash_set<std::string>& ignored_interfaces,
    base::ScopedFD netlink_fd) {
  auto ncn_linux = std::make_unique<NetworkChangeNotifierLinux>(
      ignored_interfaces, /*initialize_blocking_thread_objects=*/false,
      /*bus=*/nullptr, base::PassKey<NetworkChangeNotifierLinux>());
  ncn_linux->InitBlockingThreadObjectsForTesting(  // IN-TEST
      std::move(netlink_fd));
  return ncn_linux;
}

// static
std::unique_ptr<NetworkChangeNotifierLinux>
NetworkChangeNotifierLinux::CreateWithBusForTesting(dbus::Bus* bus) {
  return std::make_unique<NetworkChangeNotifierLinux>(
      absl::flat_hash_set<std::string>(),
      /*initialize_blocking_thread_objects=*/false, bus,
      base::PassKey<NetworkChangeNotifierLinux>());
}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(
    const absl::flat_hash_set<std::string>& ignored_interfaces)
    : NetworkChangeNotifierLinux(ignored_interfaces,
                                 /*initialize_blocking_thread_objects=*/true,
                                 /*bus=*/nullptr,
                                 base::PassKey<NetworkChangeNotifierLinux>()) {}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(
    const absl::flat_hash_set<std::string>& ignored_interfaces,
    dbus::Bus* bus)
    : NetworkChangeNotifierLinux(ignored_interfaces,
                                 /*initialize_blocking_thread_objects=*/true,
                                 bus,
                                 base::PassKey<NetworkChangeNotifierLinux>()) {}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(
    const absl::flat_hash_set<std::string>& ignored_interfaces,
    bool initialize_blocking_thread_objects,
    dbus::Bus* bus,
    base::PassKey<NetworkChangeNotifierLinux>)
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsLinux()),
      blocking_thread_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      blocking_thread_objects_(
          new BlockingThreadObjects(ignored_interfaces,
                                    blocking_thread_runner_,
                                    initialize_blocking_thread_objects),
          // Ensure |blocking_thread_objects_| lives on
          // |blocking_thread_runner_| to prevent races where
          // NetworkChangeNotifierLinux outlives
          // TaskEnvironment. https://crbug.com/938126
          base::OnTaskRunnerDeleter(blocking_thread_runner_)) {
  if (initialize_blocking_thread_objects) {
    blocking_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&NetworkChangeNotifierLinux::BlockingThreadObjects::Init,
                       // The Unretained pointer is safe here because it's
                       // posted before the deleter can post.
                       base::Unretained(blocking_thread_objects_.get())));
  }
  if (initialize_blocking_thread_objects || bus) {
    portal_monitor_ = PortalMonitor::Create(
        bus,
        base::BindRepeating(
            &NetworkChangeNotifierLinux::OnPortalOnlineStateChanged,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(
            &NetworkChangeNotifierLinux::OnPortalConnectionCostChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  portal_monitor_.reset();
  ClearGlobalPointer();
}

// static
NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierLinux::NetworkChangeCalculatorParamsLinux() {
  NetworkChangeCalculatorParams params;
  // Delay values arrived at by simple experimentation and adjusted so as to
  // produce a single signal when switching between network connections.
  params.ip_address_offline_delay_ = base::Milliseconds(2000);
  params.ip_address_online_delay_ = base::Milliseconds(2000);
  params.connection_type_offline_delay_ = base::Milliseconds(1500);
  params.connection_type_online_delay_ = base::Milliseconds(500);
  return params;
}

void NetworkChangeNotifierLinux::InitBlockingThreadObjectsForTesting(
    base::ScopedFD netlink_fd) {
  DCHECK(blocking_thread_objects_);
  blocking_thread_objects_->SetNetlinkInitializedForTesting();  // IN-TEST
  blocking_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NetworkChangeNotifierLinux::BlockingThreadObjects::InitForTesting,
          // The Unretained pointer is safe here because it's
          // posted before the deleter can post.
          base::Unretained(blocking_thread_objects_.get()),
          std::move(netlink_fd)));
}

void NetworkChangeNotifierLinux::OnPortalOnlineStateChanged(
    PortalOnlineState state) {
  blocking_thread_objects_->SetPortalOnlineState(state);
}

void NetworkChangeNotifierLinux::OnPortalConnectionCostChanged(
    ConnectionCost cost) {
  ConnectionCost old_effective_cost = GetCurrentConnectionCost();
  last_computed_connection_cost_.store(cost, std::memory_order_relaxed);
  if (GetCurrentConnectionCost() != old_effective_cost) {
    NotifyObserversOfConnectionCostChange();
  }
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierLinux::GetCurrentConnectionType() const {
  return blocking_thread_objects_->GetCurrentConnectionType();
}

NetworkChangeNotifier::ConnectionCost
NetworkChangeNotifierLinux::GetCurrentConnectionCost() {
  ConnectionCost cost =
      last_computed_connection_cost_.load(std::memory_order_relaxed);
  if (cost != ConnectionCost::CONNECTION_COST_UNKNOWN) {
    return cost;
  }
  return NetworkChangeNotifier::GetCurrentConnectionCost();
}

AddressMapOwnerLinux* NetworkChangeNotifierLinux::GetAddressMapOwnerInternal() {
  return blocking_thread_objects_->address_tracker();
}

}  // namespace net

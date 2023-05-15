// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "net/base/address_tracker_linux.h"
#include "net/dns/dns_config_service_posix.h"

namespace net {

// A collection of objects that live on blocking threads.
class NetworkChangeNotifierLinux::BlockingThreadObjects {
 public:
  explicit BlockingThreadObjects(
      const std::unordered_set<std::string>& ignored_interfaces,
      scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner);
  BlockingThreadObjects(const BlockingThreadObjects&) = delete;
  BlockingThreadObjects& operator=(const BlockingThreadObjects&) = delete;

  // Plumbing for NetworkChangeNotifier::GetCurrentConnectionType.
  // Safe to call from any thread.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() {
    return address_tracker_.GetCurrentConnectionType();
  }

  internal::AddressTrackerLinux* address_tracker() { return &address_tracker_; }

  // Begin watching for netlink changes.
  void Init();

  void InitForTesting(base::ScopedFD netlink_fd);  // IN-TEST

 private:
  void OnIPAddressChanged();
  void OnLinkChanged();
  // Used to detect online/offline state and IP address changes.
  internal::AddressTrackerLinux address_tracker_;
  NetworkChangeNotifier::ConnectionType last_type_ =
      NetworkChangeNotifier::CONNECTION_NONE;
};

NetworkChangeNotifierLinux::BlockingThreadObjects::BlockingThreadObjects(
    const std::unordered_set<std::string>& ignored_interfaces,
    scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner)
    : address_tracker_(
          base::BindRepeating(&NetworkChangeNotifierLinux::
                                  BlockingThreadObjects::OnIPAddressChanged,
                              base::Unretained(this)),
          base::BindRepeating(
              &NetworkChangeNotifierLinux::BlockingThreadObjects::OnLinkChanged,
              base::Unretained(this)),
          base::DoNothing(),
          ignored_interfaces,
          std::move(blocking_thread_runner)) {}

void NetworkChangeNotifierLinux::BlockingThreadObjects::Init() {
  address_tracker_.Init();
  last_type_ = GetCurrentConnectionType();
}

void NetworkChangeNotifierLinux::BlockingThreadObjects::InitForTesting(
    base::ScopedFD netlink_fd) {
  address_tracker_.InitWithFdForTesting(std::move(netlink_fd));  // IN-TEST
  last_type_ = GetCurrentConnectionType();
}

void NetworkChangeNotifierLinux::BlockingThreadObjects::OnIPAddressChanged() {
  NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
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
    const std::unordered_set<std::string>& ignored_interfaces,
    base::ScopedFD netlink_fd) {
  auto ncn_linux = std::make_unique<NetworkChangeNotifierLinux>(
      ignored_interfaces, /*initialize_blocking_thread_objects=*/false,
      base::PassKey<NetworkChangeNotifierLinux>());
  ncn_linux->InitBlockingThreadObjectsForTesting(  // IN-TEST
      std::move(netlink_fd));
  return ncn_linux;
}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(
    const std::unordered_set<std::string>& ignored_interfaces)
    : NetworkChangeNotifierLinux(ignored_interfaces,
                                 /*initialize_blocking_thread_objects*/ true,
                                 base::PassKey<NetworkChangeNotifierLinux>()) {}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(
    const std::unordered_set<std::string>& ignored_interfaces,
    bool initialize_blocking_thread_objects,
    base::PassKey<NetworkChangeNotifierLinux>)
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsLinux()),
      blocking_thread_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      blocking_thread_objects_(
          new BlockingThreadObjects(ignored_interfaces,
                                    blocking_thread_runner_),
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
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
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
  blocking_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NetworkChangeNotifierLinux::BlockingThreadObjects::InitForTesting,
          // The Unretained pointer is safe here because it's
          // posted before the deleter can post.
          base::Unretained(blocking_thread_objects_.get()),
          std::move(netlink_fd)));
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierLinux::GetCurrentConnectionType() const {
  return blocking_thread_objects_->GetCurrentConnectionType();
}

AddressMapOwnerLinux* NetworkChangeNotifierLinux::GetAddressMapOwnerInternal() {
  return blocking_thread_objects_->address_tracker();
}

}  // namespace net

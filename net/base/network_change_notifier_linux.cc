// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread.h"
#include "net/base/address_tracker_linux.h"
#include "net/dns/dns_config_service_posix.h"

namespace net {

// A collection of objects that live on blocking threads.
class NetworkChangeNotifierLinux::BlockingThreadObjects {
 public:
  explicit BlockingThreadObjects(
      const std::unordered_set<std::string>& ignored_interfaces);

  // Plumbing for NetworkChangeNotifier::GetCurrentConnectionType.
  // Safe to call from any thread.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() {
    return address_tracker_.GetCurrentConnectionType();
  }

  const internal::AddressTrackerLinux* address_tracker() const {
    return &address_tracker_;
  }

  // Begin watching for DNS and netlink changes.
  void Init();

 private:
  void OnIPAddressChanged();
  void OnLinkChanged();
  // Used to detect online/offline state and IP address changes.
  internal::AddressTrackerLinux address_tracker_;
  NetworkChangeNotifier::ConnectionType last_type_;

  DISALLOW_COPY_AND_ASSIGN(BlockingThreadObjects);
};

NetworkChangeNotifierLinux::BlockingThreadObjects::BlockingThreadObjects(
    const std::unordered_set<std::string>& ignored_interfaces)
    : address_tracker_(
          base::BindRepeating(&NetworkChangeNotifierLinux::
                                  BlockingThreadObjects::OnIPAddressChanged,
                              base::Unretained(this)),
          base::BindRepeating(
              &NetworkChangeNotifierLinux::BlockingThreadObjects::OnLinkChanged,
              base::Unretained(this)),
          base::DoNothing(),
          ignored_interfaces),
      last_type_(NetworkChangeNotifier::CONNECTION_NONE) {}

void NetworkChangeNotifierLinux::BlockingThreadObjects::Init() {
  address_tracker_.Init();
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

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(
    const std::unordered_set<std::string>& ignored_interfaces)
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsLinux()),
      blocking_thread_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock()})),
      blocking_thread_objects_(
          new BlockingThreadObjects(ignored_interfaces),
          // Ensure |blocking_thread_objects_| lives on
          // |blocking_thread_runner_| to prevent races where
          // NetworkChangeNotifierLinux outlives
          // TaskEnvironment. https://crbug.com/938126
          base::OnTaskRunnerDeleter(blocking_thread_runner_)) {
  blocking_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkChangeNotifierLinux::BlockingThreadObjects::Init,
                     // The Unretained pointer is safe here because it's
                     // posted before the deleter can post.
                     base::Unretained(blocking_thread_objects_.get())));
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
  params.ip_address_offline_delay_ = base::TimeDelta::FromMilliseconds(2000);
  params.ip_address_online_delay_ = base::TimeDelta::FromMilliseconds(2000);
  params.connection_type_offline_delay_ =
      base::TimeDelta::FromMilliseconds(1500);
  params.connection_type_online_delay_ = base::TimeDelta::FromMilliseconds(500);
  return params;
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierLinux::GetCurrentConnectionType() const {
  return blocking_thread_objects_->GetCurrentConnectionType();
}

const internal::AddressTrackerLinux*
NetworkChangeNotifierLinux::GetAddressTrackerInternal() const {
  return blocking_thread_objects_->address_tracker();
}

}  // namespace net

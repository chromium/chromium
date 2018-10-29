// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "net/base/address_tracker_linux.h"
#include "net/dns/dns_config_service.h"

namespace net {

class NetworkChangeNotifierLinux::Thread : public base::Thread {
 public:
  explicit Thread(const std::unordered_set<std::string>& ignored_interfaces);
  ~Thread() override;

  // Plumbing for NetworkChangeNotifier::GetCurrentConnectionType.
  // Safe to call from any thread.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() {
    return address_tracker_->GetCurrentConnectionType();
  }

  const internal::AddressTrackerLinux* address_tracker() const {
    return address_tracker_.get();
  }

 protected:
  // base::Thread
  void Init() override;
  void CleanUp() override;

 private:
  void OnIPAddressChanged();
  void OnLinkChanged();
  std::unique_ptr<DnsConfigService> dns_config_service_;
  // Used to detect online/offline state and IP address changes.
  std::unique_ptr<internal::AddressTrackerLinux> address_tracker_;
  NetworkChangeNotifier::ConnectionType last_type_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

NetworkChangeNotifierLinux::Thread::Thread(
    const std::unordered_set<std::string>& ignored_interfaces)
    : base::Thread("NetworkChangeNotifier"),
      address_tracker_(new internal::AddressTrackerLinux(
          base::Bind(&NetworkChangeNotifierLinux::Thread::OnIPAddressChanged,
                     base::Unretained(this)),
          base::Bind(&NetworkChangeNotifierLinux::Thread::OnLinkChanged,
                     base::Unretained(this)),
          base::DoNothing(),
          ignored_interfaces)),
      last_type_(NetworkChangeNotifier::CONNECTION_NONE) {}

NetworkChangeNotifierLinux::Thread::~Thread() {
  DCHECK(!Thread::IsRunning());
}

void NetworkChangeNotifierLinux::Thread::Init() {
  address_tracker_->Init();
  last_type_ = GetCurrentConnectionType();
  dns_config_service_ = DnsConfigService::CreateSystemService();
  dns_config_service_->WatchConfig(
      base::Bind(&NetworkChangeNotifier::SetDnsConfig));
}

void NetworkChangeNotifierLinux::Thread::CleanUp() {
  // Delete AddressTrackerLinux before MessageLoop gets deleted as
  // AddressTrackerLinux's FileDescriptorWatcher holds a pointer to the
  // MessageLoop.
  address_tracker_.reset();
  dns_config_service_.reset();
}

void NetworkChangeNotifierLinux::Thread::OnIPAddressChanged() {
  NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
  // When the IP address of a network interface is added/deleted, the
  // connection type may have changed.
  OnLinkChanged();
}

void NetworkChangeNotifierLinux::Thread::OnLinkChanged() {
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
      notifier_thread_(new Thread(ignored_interfaces)) {
  // We create this notifier thread because the notification implementation
  // needs a MessageLoopForIO, and there's no guarantee that
  // MessageLoopCurrent::Get() meets that criterion.
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  // Stopping from here allows us to sanity- check that the notifier
  // thread shut down properly.
  notifier_thread_->Stop();
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
  return notifier_thread_->GetCurrentConnectionType();
}

const internal::AddressTrackerLinux*
NetworkChangeNotifierLinux::GetAddressTrackerInternal() const {
  return notifier_thread_->address_tracker();
}

}  // namespace net

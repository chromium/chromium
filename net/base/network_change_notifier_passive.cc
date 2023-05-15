// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_passive.h"

#include <string>
#include <unordered_set>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/dns/dns_config_service_posix.h"
#include "net/dns/system_dns_config_change_notifier.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_change_notifier_android.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include <linux/rtnetlink.h>

#include "net/base/network_change_notifier_linux.h"
#endif

namespace net {

NetworkChangeNotifierPassive::NetworkChangeNotifierPassive(
    NetworkChangeNotifier::ConnectionType initial_connection_type,
    NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype)
    : NetworkChangeNotifierPassive(initial_connection_type,
                                   initial_connection_subtype,
                                   /*system_dns_config_notifier=*/nullptr) {}

NetworkChangeNotifierPassive::NetworkChangeNotifierPassive(
    NetworkChangeNotifier::ConnectionType initial_connection_type,
    NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier)
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsPassive(),
                            system_dns_config_notifier),
      connection_type_(initial_connection_type),
      max_bandwidth_mbps_(
          NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype(
              initial_connection_subtype)) {}

NetworkChangeNotifierPassive::~NetworkChangeNotifierPassive() {
  ClearGlobalPointer();
}

void NetworkChangeNotifierPassive::OnDNSChanged() {
  GetCurrentSystemDnsConfigNotifier()->RefreshConfig();
}

void NetworkChangeNotifierPassive::OnIPAddressChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
}

void NetworkChangeNotifierPassive::OnConnectionChanged(
    NetworkChangeNotifier::ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    base::AutoLock scoped_lock(lock_);
    connection_type_ = connection_type;
  }
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
}

void NetworkChangeNotifierPassive::OnConnectionSubtypeChanged(
    NetworkChangeNotifier::ConnectionType connection_type,
    NetworkChangeNotifier::ConnectionSubtype connection_subtype) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  double max_bandwidth_mbps =
      GetMaxBandwidthMbpsForConnectionSubtype(connection_subtype);
  {
    base::AutoLock scoped_lock(lock_);
    max_bandwidth_mbps_ = max_bandwidth_mbps;
  }
  NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChange(max_bandwidth_mbps,
                                                             connection_type);
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierPassive::GetCurrentConnectionType() const {
  base::AutoLock scoped_lock(lock_);
  return connection_type_;
}

void NetworkChangeNotifierPassive::GetCurrentMaxBandwidthAndConnectionType(
    double* max_bandwidth_mbps,
    ConnectionType* connection_type) const {
  base::AutoLock scoped_lock(lock_);
  *connection_type = connection_type_;
  *max_bandwidth_mbps = max_bandwidth_mbps_;
}

#if BUILDFLAG(IS_LINUX)
AddressMapOwnerLinux*
NetworkChangeNotifierPassive::GetAddressMapOwnerInternal() {
  return &address_map_cache_;
}
#endif

// static
NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierPassive::NetworkChangeCalculatorParamsPassive() {
  NetworkChangeCalculatorParams params;
#if BUILDFLAG(IS_CHROMEOS)
  // Delay values arrived at by simple experimentation and adjusted so as to
  // produce a single signal when switching between network connections.
  params.ip_address_offline_delay_ = base::Milliseconds(4000);
  params.ip_address_online_delay_ = base::Milliseconds(1000);
  params.connection_type_offline_delay_ = base::Milliseconds(500);
  params.connection_type_online_delay_ = base::Milliseconds(500);
#elif BUILDFLAG(IS_ANDROID)
  params = NetworkChangeNotifierAndroid::NetworkChangeCalculatorParamsAndroid();
#elif BUILDFLAG(IS_LINUX)
  params = NetworkChangeNotifierLinux::NetworkChangeCalculatorParamsLinux();
#else
  NOTIMPLEMENTED();
#endif
  return params;
}

}  // namespace net

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_PASSIVE_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_PASSIVE_H_

#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

#if BUILDFLAG(IS_LINUX)
#include "net/base/address_map_cache_linux.h"
#endif

namespace net {

// A NetworkChangeNotifier that needs to be told about network changes by some
// other object. This is useful on platforms like ChromeOS, Lacros, and Android
// where only objects running in the browser process can listen for network
// state changes, but other processes want to add observers for network state.
// It's also useful on Linux where listening for network state changes in a
// sandboxed process requires loosening the sandbox policy too much.
class NET_EXPORT NetworkChangeNotifierPassive : public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierPassive(
      NetworkChangeNotifier::ConnectionType initial_connection_type,
      NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype);
  NetworkChangeNotifierPassive(const NetworkChangeNotifierPassive&) = delete;
  NetworkChangeNotifierPassive& operator=(const NetworkChangeNotifierPassive&) =
      delete;
  ~NetworkChangeNotifierPassive() override;

  // These methods are used to notify this object that a network property has
  // changed. These must be called from the thread that owns this object.
  void OnDNSChanged();
  void OnIPAddressChanged();
  void OnConnectionChanged(
      NetworkChangeNotifier::ConnectionType connection_type);
  void OnConnectionSubtypeChanged(
      NetworkChangeNotifier::ConnectionType connection_type,
      NetworkChangeNotifier::ConnectionSubtype connection_subtype);

 protected:
  // NetworkChangeNotifier overrides.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType()
      const override;
  void GetCurrentMaxBandwidthAndConnectionType(
      double* max_bandwidth_mbps,
      ConnectionType* connection_type) const override;
#if BUILDFLAG(IS_LINUX)
  AddressMapOwnerLinux* GetAddressMapOwnerInternal() override;
#endif

 private:
  friend class NetworkChangeNotifierPassiveTest;

  // For testing purposes, allows specifying a SystemDnsConfigChangeNotifier.
  // If |system_dns_config_notifier| is nullptr, NetworkChangeNotifier create a
  // global one.
  NetworkChangeNotifierPassive(
      NetworkChangeNotifier::ConnectionType initial_connection_type,
      NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype,
      SystemDnsConfigChangeNotifier* system_dns_config_notifier);

  // Calculates parameters used for network change notifier online/offline
  // signals.
  static NetworkChangeNotifier::NetworkChangeCalculatorParams
  NetworkChangeCalculatorParamsPassive();

  THREAD_CHECKER(thread_checker_);

#if BUILDFLAG(IS_LINUX)
  AddressMapCacheLinux address_map_cache_;
#endif

  mutable base::Lock lock_;
  NetworkChangeNotifier::ConnectionType
      connection_type_;        // Guarded by |lock_|.
  double max_bandwidth_mbps_;  // Guarded by |lock_|.
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_PASSIVE_H_

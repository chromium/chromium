// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_

#include <SystemConfiguration/SystemConfiguration.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_config_watcher_mac.h"

namespace net {

class NetworkChangeNotifierMac: public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierMac();
  NetworkChangeNotifierMac(const NetworkChangeNotifierMac&) = delete;
  NetworkChangeNotifierMac& operator=(const NetworkChangeNotifierMac&) = delete;
  ~NetworkChangeNotifierMac() override;

  // NetworkChangeNotifier implementation:
  ConnectionType GetCurrentConnectionType() const override;

  // Forwarder just exists to keep the NetworkConfigWatcherMac API out of
  // NetworkChangeNotifierMac's public API.
  class Forwarder : public NetworkConfigWatcherMac::Delegate {
   public:
    explicit Forwarder(NetworkChangeNotifierMac* net_config_watcher)
        : net_config_watcher_(net_config_watcher) {}
    Forwarder(const Forwarder&) = delete;
    Forwarder& operator=(const Forwarder&) = delete;

    // NetworkConfigWatcherMac::Delegate implementation:
    void Init() override;
    void StartReachabilityNotifications() override;
    void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store) override;
    void OnNetworkConfigChange(CFArrayRef changed_keys) override;

   private:
    const raw_ptr<NetworkChangeNotifierMac> net_config_watcher_;
  };

 private:
  // Called on the main thread on startup, afterwards on the notifier thread.
  static ConnectionType CalculateConnectionType(SCNetworkConnectionFlags flags);

  // Methods directly called by the NetworkConfigWatcherMac::Delegate:
  void StartReachabilityNotifications();
  void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store);
  void OnNetworkConfigChange(CFArrayRef changed_keys);

  void SetInitialConnectionType();

  static void ReachabilityCallback(SCNetworkReachabilityRef target,
                                   SCNetworkConnectionFlags flags,
                                   void* notifier);

  static NetworkChangeCalculatorParams NetworkChangeCalculatorParamsMac();

  // These must be constructed before config_watcher_ to ensure
  // the lock is in a valid state when Forwarder::Init is called.
  ConnectionType connection_type_ = CONNECTION_UNKNOWN;
  bool connection_type_initialized_ = false;
  mutable base::Lock connection_type_lock_;
  mutable base::ConditionVariable initial_connection_type_cv_;
  base::apple::ScopedCFTypeRef<SCNetworkReachabilityRef> reachability_;
  base::apple::ScopedCFTypeRef<CFRunLoopRef> run_loop_;

  Forwarder forwarder_;
  std::unique_ptr<const NetworkConfigWatcherMac> config_watcher_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_

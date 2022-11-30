// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CONFIG_WATCHER_MAC_H_
#define NET_BASE_NETWORK_CONFIG_WATCHER_MAC_H_

#include <SystemConfiguration/SystemConfiguration.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"

namespace base {
class Thread;
}

namespace net {

// Helper class for watching the Mac OS system network settings.
class NetworkConfigWatcherMac {
 public:
  // NOTE: The lifetime of Delegate is expected to exceed the lifetime of
  // NetworkConfigWatcherMac.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called to let the delegate do any setup work the must be run on the
    // notifier thread immediately after it starts.
    virtual void Init() {}

    // Called to start receiving notifications from the SCNetworkReachability
    // API.
    // Will be called on the notifier thread.
    virtual void StartReachabilityNotifications() = 0;

    // Called to register the notification keys on |store|.
    // Implementors are expected to call SCDynamicStoreSetNotificationKeys().
    // Will be called on the notifier thread.
    virtual void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store) = 0;

    // Called when one of the notification keys has changed.
    // Will be called on the notifier thread.
    virtual void OnNetworkConfigChange(CFArrayRef changed_keys) = 0;
  };

  explicit NetworkConfigWatcherMac(Delegate* delegate);
  NetworkConfigWatcherMac(const NetworkConfigWatcherMac&) = delete;
  NetworkConfigWatcherMac& operator=(const NetworkConfigWatcherMac&) = delete;
  ~NetworkConfigWatcherMac();

 private:
  // The thread used to listen for notifications.  This relays the notification
  // to the registered observers without posting back to the thread the object
  // was created on.
  std::unique_ptr<base::Thread> notifier_thread_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CONFIG_WATCHER_MAC_H_

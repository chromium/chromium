// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_

#include <fuchsia/net/interfaces/cpp/fidl.h>

#include <optional>
#include <vector>

#include "base/threading/thread_checker.h"
#include "base/types/expected.h"
#include "net/base/fuchsia/network_interface_cache.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

class SystemDnsConfigChangeNotifier;

class NET_EXPORT_PRIVATE NetworkChangeNotifierFuchsia
    : public NetworkChangeNotifier {
 public:
  // Registers for asynchronous notifications of changes to network interfaces.
  // Only WLAN interfaces are observed if |require_wlan| is requested.
  explicit NetworkChangeNotifierFuchsia(bool require_wlan);
  NetworkChangeNotifierFuchsia(const NetworkChangeNotifierFuchsia&) = delete;
  NetworkChangeNotifierFuchsia& operator=(const NetworkChangeNotifierFuchsia&) =
      delete;
  ~NetworkChangeNotifierFuchsia() override;

  // NetworkChangeNotifier implementation.
  ConnectionType GetCurrentConnectionType() const override;

 private:
  friend class NetworkChangeNotifierFuchsiaTest;

  const internal::NetworkInterfaceCache* GetNetworkInterfaceCacheInternal()
      const override;

  NetworkChangeNotifierFuchsia(
      fuchsia::net::interfaces::WatcherHandle watcher,
      bool require_wlan,
      SystemDnsConfigChangeNotifier* system_dns_config_notifier);

  // Processes events from the watcher for interface addition, change, or
  // removal. Listeners are notified of changes that affect them. `watcher_` is
  // unbound if an event is malformed in some way.
  void OnInterfacesEvent(fuchsia::net::interfaces::Event event);

  // Notifies observers of changes. Unbinds `watcher_` if there was an error.
  void HandleCacheStatus(
      std::optional<internal::NetworkInterfaceCache::ChangeBits> change_bits);

  fuchsia::net::interfaces::WatcherPtr watcher_;

  // Keeps an updated cache of network interfaces and connection type.
  internal::NetworkInterfaceCache cache_;

  THREAD_CHECKER(thread_checker_);
};

namespace internal {

// Connects to the service via the process' ComponentContext, and connects the
// Watcher to the service.
fuchsia::net::interfaces::WatcherHandle ConnectInterfacesWatcher();

// Reads existing network interfaces from `watcher_handle`, appending them to
// `interfaces`. If successful, returns an unbound WatcherHandle that can be
// used to watch for subsequent changes.
//
// `watcher_handle` must be a newly created fuchsia.net.interfaces.Watcher. Can
// be used as the first part of the hanging-get pattern.
base::expected<fuchsia::net::interfaces::WatcherHandle, zx_status_t>
ReadExistingNetworkInterfacesFromNewWatcher(
    fuchsia::net::interfaces::WatcherHandle watcher_handle,
    std::vector<fuchsia::net::interfaces::Properties>& interfaces);

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_

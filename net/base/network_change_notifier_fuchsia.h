// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_

#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_interfaces_fuchsia.h"

namespace net {

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
  using InterfacePropertiesMap =
      base::flat_map<uint64_t, internal::InterfaceProperties>;
  friend class NetworkChangeNotifierFuchsiaTest;

  NetworkChangeNotifierFuchsia(
      fidl::InterfaceHandle<fuchsia::net::interfaces::Watcher> watcher,
      bool require_wlan,
      SystemDnsConfigChangeNotifier* system_dns_config_notifier = nullptr);

  // Processes events from the watcher for interface addition, change, or
  // removal.
  void OnInterfacesEvent(fuchsia::net::interfaces::Event event);

  // Handlers for the interface change events. Listeners are notified of changes
  // that affect them. |watcher_| is closed if an event is malformed in some
  // way.
  void OnInterfaceAdded(fuchsia::net::interfaces::Properties properties);
  void OnInterfaceRemoved(uint64_t interface_id);
  void OnInterfaceChanged(fuchsia::net::interfaces::Properties properties);

  // Unbinds the watcher, reset the connection type and logs |error_message|.
  void OnWatcherError(base::StringPiece error_message);

  // Updates the connection type from |interface_cache_| and notifies observers
  // of changes.
  void UpdateConnectionType();

  // Resets the connection type to CONNECTION_UNKNOWN.
  void ResetConnectionType();

  // Returns the ConnectionType converted from |properties|' device_class.
  // Returns CONNECTION_NONE if the interface is not publicly routable, taking
  // into account the |requires_wlan_| setting.
  ConnectionType GetEffectiveConnectionType(
      const internal::InterfaceProperties& properties);

  // Returns true if the effective connection type is not CONNECTION_NONE.
  bool CanReachExternalNetwork(const internal::InterfaceProperties& properties);

  // Whether only WLAN interfaces should be taken into account.
  const bool require_wlan_;

  fuchsia::net::interfaces::WatcherPtr watcher_;

  // The ConnectionType of the default network interface, stored as an atomic
  // 32-bit int for safe concurrent access.
  base::subtle::Atomic32 cached_connection_type_ = CONNECTION_UNKNOWN;

  InterfacePropertiesMap interface_cache_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_FUCHSIA_H_

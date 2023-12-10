// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FUCHSIA_NETWORK_INTERFACE_CACHE_H_
#define NET_BASE_FUCHSIA_NETWORK_INTERFACE_CACHE_H_

#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <stdint.h>
#include <zircon/errors.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_interfaces_fuchsia.h"

namespace net::internal {

// Cache of network interfaces, keyed by unique interface IDs, kept up-to-date
// by NetworkChangeNotifierFuchsia.
//
// If `require_wlan` is `true`, only WLAN interfaces are observed.
//
// Can be accessed via `NetworkChangeNotifier::GetNetworkInterfaceCache()` to
// get the current list of networks. Methods that read the cache are
// thread-safe, but methods that modify the cache must be in sequence.
//
// NetworkInterfaceCache expects valid write operations only, and can go into
// unrecoverable error state if `SetError()` is called, or attempted to
// - Add an interface twice.
// - Add/Change an interface with incomplete properties.
// - Change/Remove an interface unknown to the cache.
//
// After entering error state, all subsequent write operations return
// `std::nullopt`, and subsequent read operations will not return a result
// (specifically, `GetOnlineInterfaces` returns `false`, and `GetConnectionType`
// returns `CONNECTION_UNKNOWN`).
class NET_EXPORT_PRIVATE NetworkInterfaceCache {
 public:
  using ChangeBits = uint32_t;
  enum : ChangeBits {
    kNoChange = 0,
    kIpAddressChanged = 1 << 0,
    kConnectionTypeChanged = 1 << 1,
  };

  explicit NetworkInterfaceCache(bool require_wlan);
  ~NetworkInterfaceCache();

  NetworkInterfaceCache(const NetworkInterfaceCache&) = delete;
  NetworkInterfaceCache& operator=(const NetworkInterfaceCache&) = delete;

  // Returns `std::nullopt` if any of the interfaces fail to be added. See
  // `AddInterface`.
  std::optional<ChangeBits> AddInterfaces(
      std::vector<fuchsia::net::interfaces::Properties> interfaces);

  // Returns `std::nullopt` if `properties` is invalid or incomplete, or if the
  // interface already exists in the cache.
  std::optional<ChangeBits> AddInterface(
      fuchsia::net::interfaces::Properties properties);

  // Returns `std::nullopt` if `properties` is invalid or does not contain an
  // `id`, or if the interface does not exist in the cache.
  std::optional<ChangeBits> ChangeInterface(
      fuchsia::net::interfaces::Properties properties);

  // Returns `std::nullopt` if `interface_id` does not exist in the cache.
  std::optional<ChangeBits> RemoveInterface(
      InterfaceProperties::InterfaceId interface_id);

  // Set cache to unrecoverable error state and clears the cache.
  // Should be called when contents of the cache can no longer be updated to
  // reflect the state of the system.
  void SetError();

  // Thread-safe method that populates a list of online network interfaces.
  // Ignores loopback interface. Returns `false` if in error state.
  bool GetOnlineInterfaces(NetworkInterfaceList* networks) const;

  // Thread-safe method that returns the current connection type.
  // Returns `CONNECTION_UNKNOWN` if in error state.
  NetworkChangeNotifier::ConnectionType GetConnectionType() const;

 private:
  // Updates `connection_type_` from `interfaces_` and returns `true` if
  // the connection type changed.
  bool UpdateConnectionTypeWhileLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  std::optional<ChangeBits> AddInterfaceWhileLocked(
      fuchsia::net::interfaces::Properties properties)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) VALID_CONTEXT_REQUIRED(sequence_checker_);

  void SetErrorWhileLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Whether only WLAN interfaces should be taken into account.
  const bool require_wlan_;

  mutable base::Lock lock_;

  base::flat_map<InterfaceProperties::InterfaceId, InterfaceProperties>
      interfaces_ GUARDED_BY(lock_);

  // The ConnectionType of the default network interface.
  NetworkChangeNotifier::ConnectionType connection_type_ GUARDED_BY(lock_) =
      NetworkChangeNotifier::CONNECTION_NONE;

  // Set to true if any update is inconsistent with the network interfaces state
  // that is currently cached.
  bool error_state_ GUARDED_BY(lock_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net::internal

#endif  // NET_BASE_FUCHSIA_NETWORK_INTERFACE_CACHE_H_

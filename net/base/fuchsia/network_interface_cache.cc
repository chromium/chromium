// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/fuchsia/network_interface_cache.h"

#include <fuchsia/net/interfaces/cpp/fidl.h>

#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_interfaces_fuchsia.h"

namespace net::internal {
namespace {

// Returns a ConnectionType derived from the supplied InterfaceProperties:
// - CONNECTION_NONE if the interface is not publicly routable.
// - Otherwise, returns a type derived from the interface's device_class.
NetworkChangeNotifier::ConnectionType GetEffectiveConnectionType(
    const InterfaceProperties& properties,
    bool require_wlan) {
  if (!properties.IsPubliclyRoutable()) {
    return NetworkChangeNotifier::CONNECTION_NONE;
  }

  NetworkChangeNotifier::ConnectionType connection_type =
      ConvertConnectionType(properties.device_class());
  if (require_wlan &&
      connection_type != NetworkChangeNotifier::CONNECTION_WIFI) {
    return NetworkChangeNotifier::CONNECTION_NONE;
  }
  return connection_type;
}

bool CanReachExternalNetwork(const InterfaceProperties& interface,
                             bool require_wlan) {
  return GetEffectiveConnectionType(interface, require_wlan) !=
         NetworkChangeNotifier::CONNECTION_NONE;
}

}  // namespace

NetworkInterfaceCache::NetworkInterfaceCache(bool require_wlan)
    : require_wlan_(require_wlan) {}

NetworkInterfaceCache::~NetworkInterfaceCache() = default;

std::optional<NetworkInterfaceCache::ChangeBits>
NetworkInterfaceCache::AddInterfaces(
    std::vector<fuchsia::net::interfaces::Properties> interfaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);

  ChangeBits combined_changes = kNoChange;
  for (auto& interface : interfaces) {
    auto change_bits = AddInterfaceWhileLocked(std::move(interface));
    if (!change_bits.has_value()) {
      return std::nullopt;
    }
    combined_changes |= change_bits.value();
  }
  return combined_changes;
}

std::optional<NetworkInterfaceCache::ChangeBits>
NetworkInterfaceCache::AddInterface(
    fuchsia::net::interfaces::Properties properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);

  return AddInterfaceWhileLocked(std::move(properties));
}

std::optional<NetworkInterfaceCache::ChangeBits>
NetworkInterfaceCache::AddInterfaceWhileLocked(
    fuchsia::net::interfaces::Properties properties)
    EXCLUSIVE_LOCKS_REQUIRED(lock_) VALID_CONTEXT_REQUIRED(sequence_checker_) {
  if (error_state_) {
    return std::nullopt;
  }

  auto interface = InterfaceProperties::VerifyAndCreate(std::move(properties));
  if (!interface) {
    LOG(ERROR) << "Incomplete interface properties.";
    SetErrorWhileLocked();
    return std::nullopt;
  }

  if (interfaces_.find(interface->id()) != interfaces_.end()) {
    LOG(ERROR) << "Unexpected duplicate interface ID " << interface->id();
    SetErrorWhileLocked();
    return std::nullopt;
  }

  ChangeBits change_bits = kNoChange;
  if (CanReachExternalNetwork(*interface, require_wlan_)) {
    change_bits |= kIpAddressChanged;
  }
  interfaces_.emplace(interface->id(), std::move(*interface));
  if (UpdateConnectionTypeWhileLocked()) {
    change_bits |= kConnectionTypeChanged;
  }
  return change_bits;
}

std::optional<NetworkInterfaceCache::ChangeBits>
NetworkInterfaceCache::ChangeInterface(
    fuchsia::net::interfaces::Properties properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  if (error_state_) {
    return std::nullopt;
  }

  auto cache_entry = interfaces_.find(properties.id());
  if (cache_entry == interfaces_.end()) {
    LOG(ERROR) << "Unknown interface ID " << properties.id();
    SetErrorWhileLocked();
    return std::nullopt;
  }

  const bool old_can_reach =
      CanReachExternalNetwork(cache_entry->second, require_wlan_);
  const bool has_addresses = properties.has_addresses();

  if (!cache_entry->second.Update(std::move(properties))) {
    LOG(ERROR) << "Update failed";
    SetErrorWhileLocked();
    return std::nullopt;
  }

  const bool new_can_reach =
      CanReachExternalNetwork(cache_entry->second, require_wlan_);

  ChangeBits change_bits = kNoChange;
  if (has_addresses || old_can_reach != new_can_reach) {
    change_bits |= kIpAddressChanged;
  }
  if (UpdateConnectionTypeWhileLocked()) {
    change_bits |= kConnectionTypeChanged;
  }
  return change_bits;
}

std::optional<NetworkInterfaceCache::ChangeBits>
NetworkInterfaceCache::RemoveInterface(
    InterfaceProperties::InterfaceId interface_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  if (error_state_) {
    return std::nullopt;
  }

  auto cache_entry = interfaces_.find(interface_id);
  if (cache_entry == interfaces_.end()) {
    LOG(ERROR) << "Unknown interface ID " << interface_id;
    SetErrorWhileLocked();
    return std::nullopt;
  }

  ChangeBits change_bits = kNoChange;
  if (CanReachExternalNetwork(cache_entry->second, require_wlan_)) {
    change_bits |= kIpAddressChanged;
  }
  interfaces_.erase(cache_entry);
  if (UpdateConnectionTypeWhileLocked()) {
    change_bits |= kConnectionTypeChanged;
  }
  return change_bits;
}

bool NetworkInterfaceCache::GetOnlineInterfaces(
    NetworkInterfaceList* networks) const {
  DCHECK(networks);

  base::AutoLock auto_lock(lock_);
  if (error_state_) {
    return false;
  }

  for (const auto& [_, interface] : interfaces_) {
    if (!interface.online()) {
      continue;
    }
    if (interface.device_class().is_loopback()) {
      continue;
    }
    interface.AppendNetworkInterfaces(networks);
  }
  return true;
}

NetworkChangeNotifier::ConnectionType NetworkInterfaceCache::GetConnectionType()
    const {
  base::AutoLock auto_lock(lock_);
  if (error_state_) {
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }

  return connection_type_;
}

void NetworkInterfaceCache::SetError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  SetErrorWhileLocked();
}

bool NetworkInterfaceCache::UpdateConnectionTypeWhileLocked()
    EXCLUSIVE_LOCKS_REQUIRED(lock_) VALID_CONTEXT_REQUIRED(sequence_checker_) {
  NetworkChangeNotifier::ConnectionType connection_type =
      NetworkChangeNotifier::ConnectionType::CONNECTION_NONE;
  for (const auto& [_, interface] : interfaces_) {
    connection_type = GetEffectiveConnectionType(interface, require_wlan_);
    if (connection_type != NetworkChangeNotifier::CONNECTION_NONE) {
      break;
    }
  }
  if (connection_type != connection_type_) {
    connection_type_ = connection_type;
    return true;
  }
  return false;
}

void NetworkInterfaceCache::SetErrorWhileLocked()
    EXCLUSIVE_LOCKS_REQUIRED(lock_) VALID_CONTEXT_REQUIRED(sequence_checker_) {
  error_state_ = true;
  interfaces_.clear();
  interfaces_.shrink_to_fit();
}

}  // namespace net::internal

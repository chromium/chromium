// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia(bool require_wlan)
    : NetworkChangeNotifierFuchsia(internal::ConnectInterfacesWatcher(),
                                   require_wlan) {}

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia(
    fidl::InterfaceHandle<fuchsia::net::interfaces::Watcher> handle,
    bool require_wlan,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier)
    : NetworkChangeNotifier(NetworkChangeCalculatorParams(),
                            system_dns_config_notifier),
      require_wlan_(require_wlan) {
  DCHECK(handle);

  watcher_.set_error_handler(base::LogFidlErrorAndExitProcess(
      FROM_HERE, "fuchsia.net.interfaces.Watcher"));

  fuchsia::net::interfaces::WatcherSyncPtr watcher = handle.BindSync();
  absl::optional<internal::ExistingInterfaceProperties> interfaces =
      internal::GetExistingInterfaces(watcher);
  if (!interfaces)
    return;

  handle = watcher.Unbind();
  bool notify_ip_address_changed = false;
  for (const auto& interface_entry : *interfaces) {
    notify_ip_address_changed |=
        CanReachExternalNetwork(interface_entry.second);
  }
  interface_cache_ = InterfacePropertiesMap(std::move(*interfaces));

  UpdateConnectionType();
  if (notify_ip_address_changed) {
    NotifyObserversOfIPAddressChange();
  }

  // Bind to the dispatcher for the thread's MessagePump.
  zx_status_t status = watcher_.Bind(std::move(handle));
  ZX_CHECK(status == ZX_OK, status) << "Bind()";
  watcher_->Watch(
      fit::bind_member(this, &NetworkChangeNotifierFuchsia::OnInterfacesEvent));
}

NetworkChangeNotifierFuchsia::~NetworkChangeNotifierFuchsia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ClearGlobalPointer();
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierFuchsia::GetCurrentConnectionType() const {
  ConnectionType type = static_cast<ConnectionType>(
      base::subtle::Acquire_Load(&cached_connection_type_));
  return type;
}

void NetworkChangeNotifierFuchsia::OnInterfacesEvent(
    fuchsia::net::interfaces::Event event) {
  // Immediately trigger the next watch, which will happen asynchronously. If
  // event processing encounters an error it'll close the watcher channel which
  // will cancel any pending callbacks.
  watcher_->Watch(
      fit::bind_member(this, &NetworkChangeNotifierFuchsia::OnInterfacesEvent));

  switch (event.Which()) {
    case fuchsia::net::interfaces::Event::kAdded:
      OnInterfaceAdded(std::move(event.added()));
      break;
    case fuchsia::net::interfaces::Event::kRemoved:
      OnInterfaceRemoved(event.removed());
      break;
    case fuchsia::net::interfaces::Event::kChanged:
      OnInterfaceChanged(std::move(event.changed()));
      break;
    case fuchsia::net::interfaces::Event::kExisting:
    case fuchsia::net::interfaces::Event::kIdle:
      OnWatcherError(base::StringPrintf(
          "OnInterfaceEvent: unexpected event %lu.", event.Which()));
      break;
    case fuchsia::net::interfaces::Event::Invalid:
      LOG(WARNING)
          << "Invalid event received from fuchsia.net.interfaces/Watcher";
      break;
  }
}

void NetworkChangeNotifierFuchsia::OnInterfaceAdded(
    fuchsia::net::interfaces::Properties properties) {
  uint64_t id = properties.id();
  absl::optional<internal::InterfaceProperties> cache_entry =
      internal::InterfaceProperties::VerifyAndCreate(std::move(properties));
  if (!cache_entry) {
    OnWatcherError("OnInterfaceAdded: incomplete interface properties.");
    return;
  }
  if (interface_cache_.find(id) != interface_cache_.end()) {
    OnWatcherError(base::StringPrintf(
        "OnInterfaceAdded: duplicate interface ID %lu.", id));
    return;
  }
  const bool can_reach = CanReachExternalNetwork(*cache_entry);
  interface_cache_.emplace(id, std::move(*cache_entry));
  UpdateConnectionType();
  if (can_reach) {
    NotifyObserversOfIPAddressChange();
  }
}

void NetworkChangeNotifierFuchsia::OnInterfaceRemoved(uint64_t interface_id) {
  InterfacePropertiesMap::iterator cache_entry =
      interface_cache_.find(interface_id);
  if (cache_entry == interface_cache_.end()) {
    OnWatcherError(base::StringPrintf(
        "OnInterfaceRemoved: unknown interface ID %lu.", interface_id));
    return;
  }
  const bool can_reach = CanReachExternalNetwork(cache_entry->second);
  interface_cache_.erase(cache_entry);
  UpdateConnectionType();
  if (can_reach) {
    NotifyObserversOfIPAddressChange();
  }
}

void NetworkChangeNotifierFuchsia::OnInterfaceChanged(
    fuchsia::net::interfaces::Properties properties) {
  if (!properties.has_id()) {
    OnWatcherError("OnInterfaceChanged: no interface ID.");
    return;
  }
  const uint64_t id = properties.id();
  InterfacePropertiesMap::iterator cache_entry = interface_cache_.find(id);
  if (cache_entry == interface_cache_.end()) {
    OnWatcherError(base::StringPrintf(
        "OnInterfaceChanged: unknown interface ID %lu.", id));
    return;
  }
  const bool old_can_reach = CanReachExternalNetwork(cache_entry->second);
  const bool has_addresses = properties.has_addresses();
  if (!cache_entry->second.Update(std::move(properties))) {
    OnWatcherError("OnInterfaceChanged: update failed.");
    return;
  }

  UpdateConnectionType();
  const bool can_reach = CanReachExternalNetwork(cache_entry->second);
  if (has_addresses || old_can_reach != can_reach) {
    NotifyObserversOfIPAddressChange();
  }
}

void NetworkChangeNotifierFuchsia::OnWatcherError(
    base::StringPiece error_message) {
  LOG(ERROR) << error_message;
  watcher_.Unbind();
  ResetConnectionType();
}

void NetworkChangeNotifierFuchsia::UpdateConnectionType() {
  ConnectionType connection_type = ConnectionType::CONNECTION_NONE;
  for (const auto& interface : interface_cache_) {
    if (CanReachExternalNetwork(interface.second)) {
      connection_type = GetEffectiveConnectionType(interface.second);
      break;
    }
  }
  if (connection_type != GetCurrentConnectionType()) {
    base::subtle::Release_Store(&cached_connection_type_, connection_type);
    NotifyObserversOfConnectionTypeChange();
  }
}

void NetworkChangeNotifierFuchsia::ResetConnectionType() {
  base::subtle::Release_Store(&cached_connection_type_,
                              ConnectionType::CONNECTION_UNKNOWN);
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierFuchsia::GetEffectiveConnectionType(
    const internal::InterfaceProperties& properties) {
  if (!properties.IsPubliclyRoutable())
    return NetworkChangeNotifier::CONNECTION_NONE;

  NetworkChangeNotifier::ConnectionType connection_type =
      internal::ConvertConnectionType(properties.device_class());
  if (require_wlan_ &&
      connection_type != NetworkChangeNotifier::CONNECTION_WIFI) {
    return NetworkChangeNotifier::CONNECTION_NONE;
  }
  return connection_type;
}

bool NetworkChangeNotifierFuchsia::CanReachExternalNetwork(
    const internal::InterfaceProperties& properties) {
  return GetEffectiveConnectionType(properties) !=
         NetworkChangeNotifier::CONNECTION_NONE;
}

}  // namespace net

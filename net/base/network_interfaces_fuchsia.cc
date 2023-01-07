// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_fuchsia.h"

#include <lib/sys/cpp/component_context.h>

#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "net/base/network_interfaces.h"

namespace net {
namespace internal {
namespace {

IPAddress FuchsiaIpAddressToIPAddress(const fuchsia::net::IpAddress& address) {
  switch (address.Which()) {
    case fuchsia::net::IpAddress::kIpv4:
      return IPAddress(address.ipv4().addr.data(), address.ipv4().addr.size());
    case fuchsia::net::IpAddress::kIpv6:
      return IPAddress(address.ipv6().addr.data(), address.ipv6().addr.size());
    default:
      return IPAddress();
  }
}

}  // namespace

// static
absl::optional<InterfaceProperties> InterfaceProperties::VerifyAndCreate(
    fuchsia::net::interfaces::Properties properties) {
  if (!internal::VerifyCompleteInterfaceProperties(properties))
    return absl::nullopt;
  return absl::make_optional(InterfaceProperties(std::move(properties)));
}

InterfaceProperties::InterfaceProperties(
    fuchsia::net::interfaces::Properties properties)
    : properties_(std::move(properties)) {}

InterfaceProperties::InterfaceProperties(InterfaceProperties&& interface) =
    default;

InterfaceProperties& InterfaceProperties::operator=(
    InterfaceProperties&& interface) = default;

InterfaceProperties::~InterfaceProperties() = default;

bool InterfaceProperties::Update(
    fuchsia::net::interfaces::Properties properties) {
  if (!properties.has_id() || properties_.id() != properties.id()) {
    LOG(WARNING) << "Update failed: invalid properties.";
    return false;
  }

  if (properties.has_addresses()) {
    for (const auto& fidl_address : properties.addresses()) {
      if (!fidl_address.has_addr()) {
        LOG(WARNING) << "Update failed: invalid properties.";
        return false;
      }
    }
    properties_.set_addresses(std::move(*properties.mutable_addresses()));
  }

  if (properties.has_online())
    properties_.set_online(properties.online());
  if (properties.has_has_default_ipv4_route())
    properties_.set_has_default_ipv4_route(properties.has_default_ipv4_route());
  if (properties.has_has_default_ipv6_route())
    properties_.set_has_default_ipv6_route(properties.has_default_ipv6_route());

  return true;
}

void InterfaceProperties::AppendNetworkInterfaces(
    NetworkInterfaceList* interfaces) const {
  for (const auto& fidl_address : properties_.addresses()) {
    IPAddress address = FuchsiaIpAddressToIPAddress(fidl_address.addr().addr);
    if (address.empty()) {
      LOG(WARNING) << "Unknown fuchsia.net/IpAddress variant "
                   << fidl_address.addr().addr.Which();
      continue;
    }

    // TODO(crbug.com/1131220): Set correct attributes once available in
    // fuchsia::net::interfaces::Properties.
    const int kAttributes = 0;
    interfaces->emplace_back(
        properties_.name(), properties_.name(), properties_.id(),
        internal::ConvertConnectionType(properties_.device_class()),
        std::move(address), fidl_address.addr().prefix_len, kAttributes);
  }
}

bool InterfaceProperties::IsPubliclyRoutable() const {
  if (!properties_.online())
    return false;

  for (const auto& fidl_address : properties_.addresses()) {
    const IPAddress address =
        FuchsiaIpAddressToIPAddress(fidl_address.addr().addr);
    if ((address.IsIPv4() && properties_.has_default_ipv4_route() &&
         !address.IsLinkLocal()) ||
        (address.IsIPv6() && properties_.has_default_ipv6_route() &&
         address.IsPubliclyRoutable())) {
      return true;
    }
  }
  return false;
}

NetworkChangeNotifier::ConnectionType ConvertConnectionType(
    const fuchsia::net::interfaces::DeviceClass& device_class) {
  switch (device_class.Which()) {
    case fuchsia::net::interfaces::DeviceClass::kLoopback:
      return NetworkChangeNotifier::CONNECTION_NONE;
    case fuchsia::net::interfaces::DeviceClass::kDevice:
      switch (device_class.device()) {
        case fuchsia::hardware::network::DeviceClass::WLAN:
          return NetworkChangeNotifier::CONNECTION_WIFI;
        case fuchsia::hardware::network::DeviceClass::ETHERNET:
          return NetworkChangeNotifier::CONNECTION_ETHERNET;
        default:
          return NetworkChangeNotifier::CONNECTION_UNKNOWN;
      }
    default:
      LOG(WARNING) << "Received unknown fuchsia.net.interfaces/DeviceClass "
                   << device_class.Which();
      return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }
}

fuchsia::net::interfaces::WatcherHandle ConnectInterfacesWatcher() {
  fuchsia::net::interfaces::StateSyncPtr state;
  zx_status_t status =
      base::ComponentContextForProcess()->svc()->Connect(state.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect()";

  fuchsia::net::interfaces::WatcherHandle watcher;
  state->GetWatcher({} /*options*/, watcher.NewRequest());
  return watcher;
}

bool VerifyCompleteInterfaceProperties(
    const fuchsia::net::interfaces::Properties& properties) {
  if (!properties.has_id())
    return false;
  if (!properties.has_addresses())
    return false;
  for (const auto& fidl_address : properties.addresses()) {
    if (!fidl_address.has_addr())
      return false;
  }
  if (!properties.has_online())
    return false;
  if (!properties.has_device_class())
    return false;
  if (!properties.has_has_default_ipv4_route())
    return false;
  if (!properties.has_has_default_ipv6_route())
    return false;
  return true;
}

absl::optional<ExistingInterfaceProperties> GetExistingInterfaces(
    const fuchsia::net::interfaces::WatcherSyncPtr& watcher) {
  ExistingInterfaceProperties existing_interfaces;
  for (;;) {
    fuchsia::net::interfaces::Event event;
    zx_status_t status = watcher->Watch(&event);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "GetExistingInterfaces: Watch() failed";
      return absl::nullopt;
    }

    switch (event.Which()) {
      case fuchsia::net::interfaces::Event::Tag::kExisting: {
        absl::optional<InterfaceProperties> interface =
            InterfaceProperties::VerifyAndCreate(std::move(event.existing()));
        if (!interface) {
          LOG(ERROR) << "GetExistingInterfaces: Invalid kExisting event.";
          return absl::nullopt;
        }
        uint64_t id = interface->id();
        existing_interfaces.emplace_back(id, std::move(*interface));
        break;
      }
      case fuchsia::net::interfaces::Event::Tag::kIdle:
        // Idle means we've listed all the existing interfaces. We can stop
        // fetching events.
        return existing_interfaces;
      default:
        LOG(ERROR) << "GetExistingInterfaces: Unexpected event received.";
        return absl::nullopt;
    }
  }
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  DCHECK(networks);
  fuchsia::net::interfaces::WatcherHandle handle =
      internal::ConnectInterfacesWatcher();
  fuchsia::net::interfaces::WatcherSyncPtr watcher = handle.BindSync();

  // TODO(crbug.com/1131238): Use NetworkChangeNotifier's cached interface
  // list.
  absl::optional<internal::ExistingInterfaceProperties> existing_interfaces =
      internal::GetExistingInterfaces(watcher);
  if (!existing_interfaces)
    return false;
  handle = watcher.Unbind();
  for (const auto& interface_entry : *existing_interfaces) {
    if (!interface_entry.second.online()) {
      // GetNetworkList() only returns online interfaces.
      continue;
    }
    if (interface_entry.second.device_class().is_loopback()) {
      // GetNetworkList() returns all interfaces except loopback.
      continue;
    }
    interface_entry.second.AppendNetworkInterfaces(networks);
  }
  return true;
}

std::string GetWifiSSID() {
  NOTIMPLEMENTED();
  return std::string();
}

}  // namespace net

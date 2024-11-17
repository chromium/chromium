// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_fuchsia.h"

#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <zircon/types.h>

#include <optional>
#include <string>
#include <utility>

#include "base/logging.h"
#include "net/base/fuchsia/network_interface_cache.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_fuchsia.h"
#include "net/base/network_interfaces.h"

namespace net {
namespace internal {
namespace {

IPAddress FuchsiaIpAddressToIPAddress(const fuchsia::net::IpAddress& address) {
  switch (address.Which()) {
    case fuchsia::net::IpAddress::kIpv4:
      return IPAddress(address.ipv4().addr);
    case fuchsia::net::IpAddress::kIpv6:
      return IPAddress(address.ipv6().addr);
    default:
      return IPAddress();
  }
}

}  // namespace

// static
std::optional<InterfaceProperties> InterfaceProperties::VerifyAndCreate(
    fuchsia::net::interfaces::Properties properties) {
  if (!internal::VerifyCompleteInterfaceProperties(properties))
    return std::nullopt;
  return std::make_optional(InterfaceProperties(std::move(properties)));
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
    LOG(ERROR) << "Update failed: invalid properties.";
    return false;
  }

  if (properties.has_addresses()) {
    for (const auto& fidl_address : properties.addresses()) {
      if (!fidl_address.has_addr()) {
        LOG(ERROR) << "Update failed: invalid properties.";
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

    const int kAttributes = 0;
    interfaces->emplace_back(
        properties_.name(), properties_.name(), properties_.id(),
        internal::ConvertConnectionType(properties_.port_class()),
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
    const fuchsia::net::interfaces::PortClass& device_class) {
  switch (device_class.Which()) {
    case fuchsia::net::interfaces::PortClass::kLoopback:
      return NetworkChangeNotifier::CONNECTION_NONE;
    case fuchsia::net::interfaces::PortClass::kDevice:
      switch (device_class.device()) {
        case fuchsia::hardware::network::PortClass::WLAN_CLIENT:
          return NetworkChangeNotifier::CONNECTION_WIFI;
        case fuchsia::hardware::network::PortClass::ETHERNET:
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
  if (!properties.has_port_class())
    return false;
  if (!properties.has_has_default_ipv4_route())
    return false;
  if (!properties.has_has_default_ipv6_route())
    return false;
  if (!properties.has_name()) {
    return false;
  }
  return true;
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  DCHECK(networks);

  const internal::NetworkInterfaceCache* cache_ptr =
      NetworkChangeNotifier::GetNetworkInterfaceCache();
  if (cache_ptr) {
    return cache_ptr->GetOnlineInterfaces(networks);
  }

  fuchsia::net::interfaces::WatcherHandle watcher_handle =
      internal::ConnectInterfacesWatcher();
  std::vector<fuchsia::net::interfaces::Properties> interfaces;

  auto handle_or_status = internal::ReadExistingNetworkInterfacesFromNewWatcher(
      std::move(watcher_handle), interfaces);
  if (!handle_or_status.has_value()) {
    return false;
  }

  internal::NetworkInterfaceCache temp_cache(/*require_wlan=*/false);
  auto change_bits = temp_cache.AddInterfaces(std::move(interfaces));
  if (!change_bits.has_value()) {
    return false;
  }

  return temp_cache.GetOnlineInterfaces(networks);
}

std::string GetWifiSSID() {
  NOTIMPLEMENTED();
  return std::string();
}

}  // namespace net

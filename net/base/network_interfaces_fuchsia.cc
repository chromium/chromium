// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_fuchsia.h"

#include <fuchsia/netstack/cpp/fidl.h>
#include <zircon/ethernet/cpp/fidl.h>

#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/stringprintf.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"

namespace net {
namespace internal {
namespace {

using ConnectionType = NetworkChangeNotifier::ConnectionType;

ConnectionType ConvertConnectionType(
    const fuchsia::netstack::NetInterface& iface) {
  return iface.features & zircon::ethernet::INFO_FEATURE_WLAN
             ? NetworkChangeNotifier::CONNECTION_WIFI
             : NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

// Converts a Netstack NetInterface |interface| to a Chrome NetworkInterface.
// NetInterfaces may be bound to multiple IPv6 addresses. |address_index| is
// used to specify which address to use for the conversion.
//   address_index = 0: Uses NetInterface::addr, NetInterface::netmask.
//   address_index >= 1: Uses NetInterface::ipv6addrs[], with the array index
//                       offset by one.
NetworkInterface NetworkInterfaceFromAddress(
    const fuchsia::netstack::NetInterface& interface,
    size_t address_index) {
  // TODO(sergeyu): attributes field is used to return address state for IPv6
  // addresses. Currently Netstack doesn't provide this information.
  const int attributes = 0;

  IPAddress address;
  uint8_t prefix_length;
  if (address_index == 0) {
    address = NetAddressToIPAddress(interface.addr);
    prefix_length = MaskPrefixLength(NetAddressToIPAddress(interface.netmask));
  } else {
    CHECK_LE(address_index, interface.ipv6addrs->size());
    address =
        NetAddressToIPAddress(interface.ipv6addrs->at(address_index - 1).addr);
    prefix_length = interface.ipv6addrs->at(address_index - 1).prefix_len;
  }

  return NetworkInterface(*interface.name, interface.name, interface.id,
                          ConvertConnectionType(interface), address,
                          prefix_length, attributes);
}

}  // namespace

IPAddress NetAddressToIPAddress(const fuchsia::netstack::NetAddress& addr) {
  if (addr.ipv4) {
    return IPAddress(addr.ipv4->addr.data(), addr.ipv4->addr.count());
  }
  if (addr.ipv6) {
    return IPAddress(addr.ipv6->addr.data(), addr.ipv6->addr.count());
  }
  return IPAddress();
}

std::vector<NetworkInterface> NetInterfaceToNetworkInterfaces(
    const fuchsia::netstack::NetInterface& iface_in) {
  std::vector<NetworkInterface> output;

  // Check if the interface is up.
  if (!(iface_in.flags & fuchsia::netstack::NetInterfaceFlagUp))
    return output;

  // Skip loopback.
  if (iface_in.features & zircon::ethernet::INFO_FEATURE_LOOPBACK)
    return output;

  output.push_back(NetworkInterfaceFromAddress(iface_in, 0));

  // Append interface entries for all additional IPv6 addresses.
  for (size_t i = 0; i < iface_in.ipv6addrs->size(); ++i) {
    output.push_back(NetworkInterfaceFromAddress(iface_in, i + 1));
  }

  return output;
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  DCHECK(networks);

  fuchsia::netstack::NetstackSyncPtr netstack =
      base::fuchsia::ComponentContext::GetDefault()
          ->ConnectToServiceSync<fuchsia::netstack::Netstack>();

  // TODO(kmarshall): Use NetworkChangeNotifier's cached interface list.
  fidl::VectorPtr<fuchsia::netstack::NetInterface> interfaces;
  zx_status_t status = netstack->GetInterfaces(&interfaces);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "fuchsia::netstack::GetInterfaces()";
    return false;
  }

  for (auto& interface : interfaces.get()) {
    auto converted = internal::NetInterfaceToNetworkInterfaces(interface);
    std::move(converted.begin(), converted.end(),
              std::back_inserter(*networks));
  }

  return true;
}

std::string GetWifiSSID() {
  NOTIMPLEMENTED();
  return std::string();
}

}  // namespace net

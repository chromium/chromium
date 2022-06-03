// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_INTERFACE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_INTERFACE_MOJOM_TRAITS_H_

#include <string>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "services/network/public/mojom/network_interface.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::NetworkInterfaceDataView,
                 net::NetworkInterface> {
 public:
  static const std::string& name(const net::NetworkInterface& network) {
    return network.name;
  }
  static const std::string& friendly_name(
      const net::NetworkInterface& network) {
    return network.friendly_name;
  }
  static uint32_t interface_index(const net::NetworkInterface& network) {
    return network.interface_index;
  }
  static net::NetworkChangeNotifier::ConnectionType type(
      const net::NetworkInterface& network) {
    return network.type;
  }
  static net::IPAddress address(const net::NetworkInterface& network) {
    return network.address;
  }
  static uint32_t prefix_length(const net::NetworkInterface& network) {
    return network.prefix_length;
  }
  static int64_t ip_address_attributes(const net::NetworkInterface& network) {
    return network.ip_address_attributes;
  }

  static bool Read(network::mojom::NetworkInterfaceDataView network,
                   net::NetworkInterface* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::ConnectionType,
               net::NetworkChangeNotifier::ConnectionType> {
  static network::mojom::ConnectionType ToMojom(
      net::NetworkChangeNotifier::ConnectionType input);
  static bool FromMojom(network::mojom::ConnectionType input,
                        net::NetworkChangeNotifier::ConnectionType* output);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_INTERFACE_MOJOM_TRAITS_H_

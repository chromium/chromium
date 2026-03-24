// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_interface_mojom_traits.h"

#include "base/notreached.h"
#include "services/network/public/cpp/ip_address_mojom_traits.h"

namespace mojo {

bool StructTraits<
    network::mojom::NetworkInterfaceDataView,
    net::NetworkInterface>::Read(network::mojom::NetworkInterfaceDataView data,
                                 net::NetworkInterface* out) {
  if (!data.ReadName(&out->name)) {
    return false;
  }
  if (!data.ReadFriendlyName(&out->friendly_name)) {
    return false;
  }
  if (!data.ReadAddress(&out->address)) {
    return false;
  }
  if (!data.ReadType(&out->type)) {
    return false;
  }

  mojo::ArrayDataView<uint8_t> view;
  data.GetMacAddressDataView(&view);
  out->mac_address.emplace();
  if (!view.is_null()) {
    if (view.size() != out->mac_address->size()) {
      return false;
    }
    std::copy_n(view.data(), out->mac_address->size(),
                out->mac_address->begin());
  } else {
    out->mac_address.reset();
  }

  out->interface_index = data.interface_index();
  out->prefix_length = data.prefix_length();
  out->ip_address_attributes = data.ip_address_attributes();
  return true;
}

network::mojom::ConnectionType
EnumTraits<network::mojom::ConnectionType,
           net::NetworkChangeNotifier::ConnectionType>::
    ToMojom(net::NetworkChangeNotifier::ConnectionType input) {
  switch (input) {
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN:
      return network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET:
      return network::mojom::ConnectionType::CONNECTION_ETHERNET;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI:
      return network::mojom::ConnectionType::CONNECTION_WIFI;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G:
      return network::mojom::ConnectionType::CONNECTION_2G;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G:
      return network::mojom::ConnectionType::CONNECTION_3G;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G:
      return network::mojom::ConnectionType::CONNECTION_4G;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G:
      return network::mojom::ConnectionType::CONNECTION_5G;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE:
      return network::mojom::ConnectionType::CONNECTION_NONE;
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_BLUETOOTH:
      return network::mojom::ConnectionType::CONNECTION_BLUETOOTH;
  }
  NOTREACHED();
}

net::NetworkChangeNotifier::ConnectionType
EnumTraits<network::mojom::ConnectionType,
           net::NetworkChangeNotifier::ConnectionType>::
    FromMojom(network::mojom::ConnectionType input) {
  switch (input) {
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN;
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET;
    case network::mojom::ConnectionType::CONNECTION_WIFI:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI;
    case network::mojom::ConnectionType::CONNECTION_2G:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G;
    case network::mojom::ConnectionType::CONNECTION_3G:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G;
    case network::mojom::ConnectionType::CONNECTION_4G:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G;
    case network::mojom::ConnectionType::CONNECTION_5G:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G;
    case network::mojom::ConnectionType::CONNECTION_NONE:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE;
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return net::NetworkChangeNotifier::ConnectionType::CONNECTION_BLUETOOTH;
  }
  NOTREACHED();
}

}  // namespace mojo

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_interface_mojom_traits.h"

#include "services/network/public/cpp/ip_address_mojom_traits.h"

namespace mojo {

bool StructTraits<
    network::mojom::NetworkInterfaceDataView,
    net::NetworkInterface>::Read(network::mojom::NetworkInterfaceDataView data,
                                 net::NetworkInterface* out) {
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadFriendlyName(&out->friendly_name))
    return false;
  if (!data.ReadAddress(&out->address))
    return false;
  if (!data.ReadType(&out->type))
    return false;

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
  NOTREACHED_IN_MIGRATION();
  return network::mojom::ConnectionType::CONNECTION_UNKNOWN;
}

bool EnumTraits<network::mojom::ConnectionType,
                net::NetworkChangeNotifier::ConnectionType>::
    FromMojom(network::mojom::ConnectionType input,
              net::NetworkChangeNotifier::ConnectionType* output) {
  switch (input) {
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN;
      return true;
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET;
      return true;
    case network::mojom::ConnectionType::CONNECTION_WIFI:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI;
      return true;
    case network::mojom::ConnectionType::CONNECTION_2G:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G;
      return true;
    case network::mojom::ConnectionType::CONNECTION_3G:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G;
      return true;
    case network::mojom::ConnectionType::CONNECTION_4G:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G;
      return true;
    case network::mojom::ConnectionType::CONNECTION_5G:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G;
      return true;
    case network::mojom::ConnectionType::CONNECTION_NONE:
      *output = net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE;
      return true;
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      *output =
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_BLUETOOTH;
      return true;
  }
  return false;
}

}  // namespace mojo

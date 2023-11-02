// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/ip_address.mojom-shared.h"

namespace mojo {
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_IP_ADDRESS)
    StructTraits<network::mojom::IPAddressDataView, net::IPAddress> {
  static base::span<const uint8_t> address_bytes(
      const net::IPAddress& ip_address) {
    return ip_address.bytes();
  }

  static bool Read(network::mojom::IPAddressDataView obj, net::IPAddress* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_MOJOM_TRAITS_H_

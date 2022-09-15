// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_ENDPOINT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_ENDPOINT_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/ip_endpoint.mojom-shared.h"

namespace mojo {
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_IP_ADDRESS)
    StructTraits<network::mojom::IPEndPointDataView, net::IPEndPoint> {
  static const net::IPAddress& address(const net::IPEndPoint& obj) {
    return obj.address();
  }
  static uint16_t port(const net::IPEndPoint& obj) { return obj.port(); }

  static bool Read(network::mojom::IPEndPointDataView obj,
                   net::IPEndPoint* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_ENDPOINT_MOJOM_TRAITS_H_

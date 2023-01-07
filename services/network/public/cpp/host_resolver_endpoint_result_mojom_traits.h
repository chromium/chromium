// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_ENDPOINT_RESULT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_ENDPOINT_RESULT_MOJOM_TRAITS_H_

#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/mojom/host_resolver_endpoint_result.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_IP_ADDRESS)
    StructTraits<network::mojom::HostResolverEndpointResultDataView,
                 net::HostResolverEndpointResult> {
  static const std::vector<net::IPEndPoint>& ip_endpoints(
      const net::HostResolverEndpointResult& obj) {
    return obj.ip_endpoints;
  }

  static const net::ConnectionEndpointMetadata& metadata(
      const net::HostResolverEndpointResult& obj) {
    return obj.metadata;
  }

  static bool Read(network::mojom::HostResolverEndpointResultDataView data,
                   net::HostResolverEndpointResult* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_ENDPOINT_RESULT_MOJOM_TRAITS_H_

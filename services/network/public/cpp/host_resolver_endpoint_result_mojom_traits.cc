// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/host_resolver_endpoint_result_mojom_traits.h"
#include "services/network/public/cpp/connection_endpoint_metadata_mojom_traits.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::HostResolverEndpointResultDataView,
                  net::HostResolverEndpointResult>::
    Read(network::mojom::HostResolverEndpointResultDataView data,
         net::HostResolverEndpointResult* out) {
  if (!data.ReadIpEndpoints(&out->ip_endpoints))
    return false;

  if (!data.ReadMetadata(&out->metadata))
    return false;

  return true;
}

}  // namespace mojo

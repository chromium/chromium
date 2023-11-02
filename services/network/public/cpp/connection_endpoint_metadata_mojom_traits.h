// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ENDPOINT_METADATA_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ENDPOINT_METADATA_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/connection_endpoint_metadata.h"
#include "services/network/public/mojom/connection_endpoint_metadata.mojom-shared.h"

namespace mojo {
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_IP_ADDRESS)
    StructTraits<network::mojom::ConnectionEndpointMetadataDataView,
                 net::ConnectionEndpointMetadata> {
  using EchConfigList = std::vector<std::uint8_t>;
  static const std::vector<std::string>& supported_protocol_alpns(
      const net::ConnectionEndpointMetadata& obj) {
    return obj.supported_protocol_alpns;
  }
  static const EchConfigList& ech_config_list(
      const net::ConnectionEndpointMetadata& obj) {
    return obj.ech_config_list;
  }
  static const std::string& target_name(
      const net::ConnectionEndpointMetadata& obj) {
    return obj.target_name;
  }

  static bool Read(network::mojom::ConnectionEndpointMetadataDataView obj,
                   net::ConnectionEndpointMetadata* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ENDPOINT_METADATA_MOJOM_TRAITS_H_

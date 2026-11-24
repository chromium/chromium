// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_endpoint_metadata_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::ConnectionEndpointMetadataDataView,
                  net::ConnectionEndpointMetadata>::
    Read(network::mojom::ConnectionEndpointMetadataDataView data,
         net::ConnectionEndpointMetadata* out) {
  if (!data.ReadSupportedProtocolAlpns(&out->supported_protocol_alpns))
    return false;
  if (!data.ReadEchConfigList(&out->ech_config_list))
    return false;
  if (!data.ReadTargetName(&out->target_name))
    return false;
  return true;
}

}  // namespace mojo

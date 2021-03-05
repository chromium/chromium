// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

namespace mojo {

bool StructTraits<network::mojom::NetworkIsolationKeyDataView,
                  net::NetworkIsolationKey>::
    Read(network::mojom::NetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  base::Optional<net::SchemefulSite> top_frame_site, frame_site;
  if (!data.ReadTopFrameSite(&top_frame_site))
    return false;
  if (!data.ReadFrameSite(&frame_site))
    return false;
  // A key is either fully empty or fully populated.
  if (top_frame_site.has_value() != frame_site.has_value())
    return false;
  if (top_frame_site.has_value()) {
    *out = net::NetworkIsolationKey(std::move(top_frame_site.value()),
                                    std::move(frame_site.value()));
  } else {
    *out = net::NetworkIsolationKey();
  }
  out->opaque_and_non_transient_ = data.opaque_and_non_transient();

  // If opaque_and_non_transient_ is set, then the key must also be opaque.
  // Otherwise, the key is not valid.
  return !out->opaque_and_non_transient_ || out->IsOpaque();
}

}  // namespace mojo

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "net/base/features.h"

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
  // A key is either fully empty or fully populated (for all fields relevant
  // given the flags set).  The constructor verifies this, so if the top-frame
  // origin is populated, we call the full constructor, otherwise, the empty.
  if (top_frame_site.has_value()) {
    // We need a dummy value when the initiating_frame_origin is empty,
    // indicating that the flag to popuate it in the key was not set.
    if (!frame_site.has_value()) {
      DCHECK(!base::FeatureList::IsEnabled(
          net::features::kAppendFrameOriginToNetworkIsolationKey));
      frame_site = net::SchemefulSite();
    }
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

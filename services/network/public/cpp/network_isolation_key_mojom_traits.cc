// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/unguessable_token.h"
#include "net/base/features.h"

namespace mojo {

bool StructTraits<network::mojom::NetworkIsolationKeyDataView,
                  net::NetworkIsolationKey>::
    Read(network::mojom::NetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  absl::optional<net::SchemefulSite> top_frame_site, frame_site;

  if (!data.ReadTopFrameSite(&top_frame_site) ||
      (net::NetworkIsolationKey::IsFrameSiteEnabled() &&
       !data.ReadFrameSite(&frame_site))) {
    return false;
  }

  // A key is either fully empty or fully populated, or double keyed.
  if ((top_frame_site.has_value() != frame_site.has_value()) &&
      net::NetworkIsolationKey::IsFrameSiteEnabled())
    return false;

  absl::optional<base::UnguessableToken> nonce;
  if (!data.ReadNonce(&nonce))
    return false;

  if (!top_frame_site.has_value()) {
    // If there is a nonce, then the sites must be populated.
    if (nonce.has_value())
      return false;
    *out = net::NetworkIsolationKey();
  } else {
    *out = net::NetworkIsolationKey(std::move(top_frame_site.value()),
                                    frame_site.has_value()
                                        ? std::move(frame_site.value())
                                        : net::SchemefulSite(),
                                    nonce ? &nonce.value() : nullptr);
  }

  return true;
}

}  // namespace mojo

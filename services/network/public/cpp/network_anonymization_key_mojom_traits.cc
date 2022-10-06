// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_anonymization_key_mojom_traits.h"

#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

bool StructTraits<network::mojom::NetworkAnonymizationKeyDataView,
                  net::NetworkAnonymizationKey>::
    Read(network::mojom::NetworkAnonymizationKeyDataView data,
         net::NetworkAnonymizationKey* out) {
  absl::optional<net::SchemefulSite> top_frame_site, frame_site;

  // If we fail to parse sites that we expect to be populated return false.
  if (!data.ReadTopFrameSite(&top_frame_site) ||
      (net::NetworkAnonymizationKey::IsFrameSiteEnabled() &&
       !data.ReadFrameSite(&frame_site))) {
    return false;
  }

  // Read is_cross_site boolean flag value.
  absl::optional<bool> is_cross_site = absl::nullopt;
  if (net::NetworkAnonymizationKey::IsCrossSiteFlagSchemeEnabled() &&
      top_frame_site.has_value()) {
    is_cross_site = data.is_cross_site();
  }

  // Read nonce value.
  absl::optional<base::UnguessableToken> nonce;
  if (!data.ReadNonce(&nonce)) {
    return false;
  }

  // If a non-empty key has frame_site is enabled it must be populated.
  if (top_frame_site.has_value() &&
      net::NetworkAnonymizationKey::IsFrameSiteEnabled() &&
      !frame_site.has_value()) {
    return false;
  }

  // If top_frame_site is not populated, the entire key must be empty.
  if (!top_frame_site.has_value()) {
    if (frame_site.has_value() || nonce.has_value()) {
      return false;
    }
    *out = net::NetworkAnonymizationKey();
  } else {
    *out = net::NetworkAnonymizationKey(std::move(top_frame_site.value()),
                                        std::move(frame_site), is_cross_site,
                                        nonce);
  }

  return true;
}

}  // namespace mojo

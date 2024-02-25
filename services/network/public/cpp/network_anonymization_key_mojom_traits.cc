// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_anonymization_key_mojom_traits.h"

#include <optional>

#include "base/unguessable_token.h"
#include "net/base/features.h"

namespace mojo {

// static
bool StructTraits<network::mojom::EmptyNetworkAnonymizationKeyDataView,
                  net::NetworkAnonymizationKey>::
    Read(network::mojom::EmptyNetworkAnonymizationKeyDataView data,
         net::NetworkAnonymizationKey* out) {
  *out = net::NetworkAnonymizationKey();
  return true;
}

// static
bool StructTraits<network::mojom::NonEmptyNetworkAnonymizationKeyDataView,
                  net::NetworkAnonymizationKey>::
    Read(network::mojom::NonEmptyNetworkAnonymizationKeyDataView data,
         net::NetworkAnonymizationKey* out) {
  net::SchemefulSite top_frame_site;
  // Read is_cross_site boolean flag value.
  bool is_cross_site = data.is_cross_site();
  std::optional<base::UnguessableToken> nonce;

  if (!data.ReadTopFrameSite(&top_frame_site) || !data.ReadNonce(&nonce)) {
    return false;
  }

  *out = net::NetworkAnonymizationKey::CreateFromParts(
      std::move(top_frame_site), is_cross_site, std::move(nonce));
  return true;
}

// static
bool UnionTraits<network::mojom::NetworkAnonymizationKeyDataView,
                 net::NetworkAnonymizationKey>::
    Read(network::mojom::NetworkAnonymizationKeyDataView data,
         net::NetworkAnonymizationKey* out) {
  if (data.is_empty()) {
    return data.ReadEmpty(out);
  }

  if (!data.is_non_empty()) {
    return false;
  }

  return data.ReadNonEmpty(out);
}

// static
network::mojom::NetworkAnonymizationKeyDataView::Tag UnionTraits<
    network::mojom::NetworkAnonymizationKeyDataView,
    net::NetworkAnonymizationKey>::GetTag(const net::NetworkAnonymizationKey&
                                              network_anonymization_key) {
  if (network_anonymization_key.IsEmpty()) {
    return network::mojom::NetworkAnonymizationKeyDataView::Tag::kEmpty;
  }

  return network::mojom::NetworkAnonymizationKeyDataView::Tag::kNonEmpty;
}

}  // namespace mojo

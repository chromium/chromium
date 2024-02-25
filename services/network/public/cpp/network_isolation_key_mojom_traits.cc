// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/unguessable_token.h"
#include "net/base/features.h"

namespace mojo {

// static
bool StructTraits<network::mojom::EmptyNetworkIsolationKeyDataView,
                  net::NetworkIsolationKey>::
    Read(network::mojom::EmptyNetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  *out = net::NetworkIsolationKey();
  return true;
}

// static
bool StructTraits<network::mojom::NonEmptyNetworkIsolationKeyDataView,
                  net::NetworkIsolationKey>::
    Read(network::mojom::NonEmptyNetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  net::SchemefulSite top_frame_site;
  net::SchemefulSite frame_site;
  std::optional<base::UnguessableToken> nonce;

  if (!data.ReadTopFrameSite(&top_frame_site) ||
      !data.ReadFrameSite(&frame_site) || !data.ReadNonce(&nonce)) {
    return false;
  }

  *out = net::NetworkIsolationKey(std::move(top_frame_site),
                                  std::move(frame_site), std::move(nonce));
  return true;
}

// static
bool UnionTraits<network::mojom::NetworkIsolationKeyDataView,
                 net::NetworkIsolationKey>::
    Read(network::mojom::NetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  if (data.is_empty()) {
    return data.ReadEmpty(out);
  }

  CHECK(data.is_non_empty());
  return data.ReadNonEmpty(out);
}

// static
network::mojom::NetworkIsolationKeyDataView::Tag
UnionTraits<network::mojom::NetworkIsolationKeyDataView,
            net::NetworkIsolationKey>::GetTag(const net::NetworkIsolationKey&
                                                  network_isolation_key) {
  if (network_isolation_key.IsEmpty()) {
    return network::mojom::NetworkIsolationKeyDataView::Tag::kEmpty;
  }

  return network::mojom::NetworkIsolationKeyDataView::Tag::kNonEmpty;
}

}  // namespace mojo

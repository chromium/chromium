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
bool StructTraits<network::mojom::FrameSiteEnabledNetworkIsolationKeyDataView,
                  net::NetworkIsolationKey>::
    Read(network::mojom::FrameSiteEnabledNetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  net::SchemefulSite top_frame_site;
  net::SchemefulSite frame_site;
  absl::optional<base::UnguessableToken> nonce;

  if (!data.ReadTopFrameSite(&top_frame_site) ||
      !data.ReadFrameSite(&frame_site) || !data.ReadNonce(&nonce)) {
    return false;
  }

  *out = net::NetworkIsolationKey(std::move(top_frame_site),
                                  std::move(frame_site), std::move(nonce));
  return true;
}

// static
bool StructTraits<
    network::mojom::CrossSiteFlagEnabledNetworkIsolationKeyDataView,
    net::NetworkIsolationKey>::
    Read(network::mojom::CrossSiteFlagEnabledNetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  net::SchemefulSite top_frame_site;
  absl::optional<base::UnguessableToken> nonce;

  if (!data.ReadTopFrameSite(&top_frame_site) || !data.ReadNonce(&nonce)) {
    return false;
  }

  *out = net::NetworkIsolationKey(
      net::NetworkIsolationKey::SerializationPasskey(),
      std::move(top_frame_site), data.is_cross_site(), std::move(nonce));
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

  switch (net::NetworkIsolationKey::GetMode()) {
    case net::NetworkIsolationKey::Mode::kFrameSiteEnabled:
      if (!data.is_frame_site_enabled()) {
        return false;
      }
      return data.ReadFrameSiteEnabled(out);
    case net::NetworkIsolationKey::Mode::kCrossSiteFlagEnabled:
      if (!data.is_cross_site_flag_enabled()) {
        return false;
      }
      return data.ReadCrossSiteFlagEnabled(out);
  }
  NOTREACHED_NORETURN();
}

// static
network::mojom::NetworkIsolationKeyDataView::Tag
UnionTraits<network::mojom::NetworkIsolationKeyDataView,
            net::NetworkIsolationKey>::GetTag(const net::NetworkIsolationKey&
                                                  network_isolation_key) {
  if (network_isolation_key.IsEmpty()) {
    return network::mojom::NetworkIsolationKeyDataView::Tag::kEmpty;
  }
  switch (net::NetworkIsolationKey::GetMode()) {
    case net::NetworkIsolationKey::Mode::kFrameSiteEnabled:
      return network::mojom::NetworkIsolationKeyDataView::Tag::
          kFrameSiteEnabled;
    case net::NetworkIsolationKey::Mode::kCrossSiteFlagEnabled:
      return network::mojom::NetworkIsolationKeyDataView::Tag::
          kCrossSiteFlagEnabled;
  }
  NOTREACHED_NORETURN();
}

}  // namespace mojo

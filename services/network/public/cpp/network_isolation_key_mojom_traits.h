// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_

#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "services/network/public/mojom/network_isolation_key.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::EmptyNetworkIsolationKeyDataView,
                 net::NetworkIsolationKey> {
  static bool Read(network::mojom::EmptyNetworkIsolationKeyDataView data,
                   net::NetworkIsolationKey* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::FrameSiteEnabledNetworkIsolationKeyDataView,
                 net::NetworkIsolationKey> {
  static const net::SchemefulSite& top_frame_site(
      const net::NetworkIsolationKey& input) {
    return input.GetTopFrameSite().value();
  }

  static const net::SchemefulSite& frame_site(
      const net::NetworkIsolationKey& input) {
    return input
        .GetFrameSiteForSerialization(
            net::NetworkIsolationKey::SerializationPasskey())
        .value();
  }

  static const absl::optional<base::UnguessableToken>& nonce(
      const net::NetworkIsolationKey& input) {
    return input.GetNonce();
  }

  static bool Read(
      network::mojom::FrameSiteEnabledNetworkIsolationKeyDataView data,
      net::NetworkIsolationKey* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) StructTraits<
    network::mojom::CrossSiteFlagEnabledNetworkIsolationKeyDataView,
    net::NetworkIsolationKey> {
  static const net::SchemefulSite& top_frame_site(
      const net::NetworkIsolationKey& input) {
    return input.GetTopFrameSite().value();
  }

  static bool is_cross_site(const net::NetworkIsolationKey& input) {
    absl::optional<bool> is_cross_site = input.GetIsCrossSiteForSerialization(
        net::NetworkIsolationKey::SerializationPasskey());
    return is_cross_site.value();
  }

  static const absl::optional<base::UnguessableToken>& nonce(
      const net::NetworkIsolationKey& input) {
    return input.GetNonce();
  }

  static bool Read(
      network::mojom::CrossSiteFlagEnabledNetworkIsolationKeyDataView data,
      net::NetworkIsolationKey* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    UnionTraits<network::mojom::NetworkIsolationKeyDataView,
                net::NetworkIsolationKey> {
  static const net::NetworkIsolationKey& empty(
      const net::NetworkIsolationKey& input) {
    return input;
  }

  static const net::NetworkIsolationKey& frame_site_enabled(
      const net::NetworkIsolationKey& input) {
    return input;
  }

  static const net::NetworkIsolationKey& cross_site_flag_enabled(
      const net::NetworkIsolationKey& input) {
    return input;
  }

  static bool Read(network::mojom::NetworkIsolationKeyDataView data,
                   net::NetworkIsolationKey* out);

  static network::mojom::NetworkIsolationKeyDataView::Tag GetTag(
      const net::NetworkIsolationKey& input);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_

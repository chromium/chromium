// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ANONYMIZATION_KEY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ANONYMIZATION_KEY_MOJOM_TRAITS_H_

#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "services/network/public/mojom/network_anonymization_key.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::NetworkAnonymizationKeyDataView,
                 net::NetworkAnonymizationKey> {
  static const absl::optional<net::SchemefulSite>& top_frame_site(
      const net::NetworkAnonymizationKey& input) {
    return input.GetTopFrameSite();
  }

  static bool is_cross_site(const net::NetworkAnonymizationKey& input) {
    return input.IsCrossSite();
  }

  static const absl::optional<base::UnguessableToken>& nonce(
      const net::NetworkAnonymizationKey& input) {
    return input.GetNonce();
  }

  static bool Read(network::mojom::NetworkAnonymizationKeyDataView data,
                   net::NetworkAnonymizationKey* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ANONYMIZATION_KEY_MOJOM_TRAITS_H_

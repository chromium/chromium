// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "services/network/public/mojom/network_isolation_key.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::NetworkIsolationKeyDataView,
                 net::NetworkIsolationKey> {
  static const base::Optional<net::SchemefulSite>& top_frame_site(
      const net::NetworkIsolationKey& input) {
    return input.GetTopFrameSite();
  }

  static const base::Optional<net::SchemefulSite>& frame_site(
      const net::NetworkIsolationKey& input) {
    return input.GetFrameSite();
  }

  static bool opaque_and_non_transient(const net::NetworkIsolationKey& input) {
    return input.opaque_and_non_transient_;
  }

  static bool Read(network::mojom::NetworkIsolationKeyDataView data,
                   net::NetworkIsolationKey* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SCHEMEFUL_SITE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SCHEMEFUL_SITE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/schemeful_site.mojom-shared.h"
#include "url/mojom/origin_mojom_traits.h"

namespace url {
class Origin;
}  // namespace url

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_SCHEMEFUL)
    StructTraits<network::mojom::SchemefulSiteDataView, net::SchemefulSite> {
  static const url::Origin& site_as_origin(const net::SchemefulSite& input) {
    return input.site_as_origin_;
  }

  static bool Read(network::mojom::SchemefulSiteDataView data,
                   net::SchemefulSite* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SCHEMEFUL_SITE_MOJOM_TRAITS_H_

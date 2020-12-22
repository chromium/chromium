// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SITE_FOR_COOKIES_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SITE_FOR_COOKIES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "services/network/public/mojom/site_for_cookies.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::SiteForCookiesDataView, net::SiteForCookies> {
  static const net::SchemefulSite& site(const net::SiteForCookies& input) {
    return input.site();
  }

  static bool schemefully_same(const net::SiteForCookies& input) {
    return input.schemefully_same();
  }

  static bool Read(network::mojom::SiteForCookiesDataView data,
                   net::SiteForCookies* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SITE_FOR_COOKIES_MOJOM_TRAITS_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/site_for_cookies_mojom_traits.h"

#include "services/network/public/cpp/crash_keys.h"

namespace mojo {

bool StructTraits<network::mojom::SiteForCookiesDataView, net::SiteForCookies>::
    Read(network::mojom::SiteForCookiesDataView data,
         net::SiteForCookies* out) {
  net::SchemefulSite site;
  if (!data.ReadSite(&site)) {
    return false;
  }

  bool result =
      net::SiteForCookies::FromWire(site, data.schemefully_same(), out);
  if (!result) {
    network::debug::SetDeserializationCrashKeyString("site_for_cookie");
  }
  return result;
}

}  // namespace mojo

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_shared_mojom_traits.h"

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

bool StructTraits<network::mojom::CookieInclusionStatusDataView,
                  net::CookieInclusionStatus>::
    Read(network::mojom::CookieInclusionStatusDataView status,
         net::CookieInclusionStatus* out) {
  out->set_exclusion_reasons(status.exclusion_reasons());
  out->set_warning_reasons(status.warning_reasons());

  return net::CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      status.exclusion_reasons(), status.warning_reasons());
}

}  // namespace mojo

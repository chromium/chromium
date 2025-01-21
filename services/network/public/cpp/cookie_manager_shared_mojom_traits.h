// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_SHARED_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_SHARED_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"
#include "services/network/public/mojom/site_for_cookies.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_COOKIES)
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

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_COOKIES)
    EnumTraits<network::mojom::CookieExemptionReason,
               net::CookieInclusionStatus::ExemptionReason> {
  static network::mojom::CookieExemptionReason ToMojom(
      net::CookieInclusionStatus::ExemptionReason input);

  static bool FromMojom(network::mojom::CookieExemptionReason input,
                        net::CookieInclusionStatus::ExemptionReason* output);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_COOKIES)
    StructTraits<network::mojom::ExclusionReasonsDataView,
                 net::CookieInclusionStatus::ExclusionReasonBitset> {
  static uint64_t exclusions_bitmask(
      const net::CookieInclusionStatus::ExclusionReasonBitset& s) {
    return s.ToEnumBitmask();
  }
  static bool Read(network::mojom::ExclusionReasonsDataView view,
                   net::CookieInclusionStatus::ExclusionReasonBitset* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_COOKIES)
    StructTraits<network::mojom::WarningReasonsDataView,
                 net::CookieInclusionStatus::WarningReasonBitset> {
  static uint64_t warnings_bitmask(
      const net::CookieInclusionStatus::WarningReasonBitset& s) {
    return s.ToEnumBitmask();
  }
  static bool Read(network::mojom::WarningReasonsDataView view,
                   net::CookieInclusionStatus::WarningReasonBitset* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_COOKIES)
    StructTraits<network::mojom::CookieInclusionStatusDataView,
                 net::CookieInclusionStatus> {
  static net::CookieInclusionStatus::ExclusionReasonBitset exclusion_reasons(
      const net::CookieInclusionStatus& s) {
    return s.exclusion_reasons();
  }
  static net::CookieInclusionStatus::WarningReasonBitset warning_reasons(
      const net::CookieInclusionStatus& s) {
    return s.warning_reasons();
  }
  static net::CookieInclusionStatus::ExemptionReason exemption_reason(
      const net::CookieInclusionStatus& s) {
    return s.exemption_reason();
  }
  static bool Read(network::mojom::CookieInclusionStatusDataView status,
                   net::CookieInclusionStatus* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_SHARED_MOJOM_TRAITS_H_

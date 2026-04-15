// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_shared_mojom_traits.h"

#include "net/cookies/cookie_inclusion_status.h"
#include "services/network/public/cpp/crash_keys.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"

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

network::mojom::CookieExemptionReason
EnumTraits<network::mojom::CookieExemptionReason,
           net::CookieInclusionStatus::ExemptionReason>::
    ToMojom(net::CookieInclusionStatus::ExemptionReason input) {
  switch (input) {
    case net::CookieInclusionStatus::ExemptionReason::kNone:
      return network::mojom::CookieExemptionReason::kNone;
    case net::CookieInclusionStatus::ExemptionReason::kUserSetting:
      return network::mojom::CookieExemptionReason::kUserSetting;
    case net::CookieInclusionStatus::ExemptionReason::kEnterprisePolicy:
      return network::mojom::CookieExemptionReason::kEnterprisePolicy;
    case net::CookieInclusionStatus::ExemptionReason::kStorageAccess:
      return network::mojom::CookieExemptionReason::kStorageAccess;
    case net::CookieInclusionStatus::ExemptionReason::kTopLevelStorageAccess:
      return network::mojom::CookieExemptionReason::kTopLevelStorageAccess;
    case net::CookieInclusionStatus::ExemptionReason::kScheme:
      return network::mojom::CookieExemptionReason::kScheme;
    case net::CookieInclusionStatus::ExemptionReason::
        kSameSiteNoneCookiesInSandbox:
      return network::mojom::CookieExemptionReason::
          kSameSiteNoneCookiesInSandbox;
  }
  NOTREACHED();
}

net::CookieInclusionStatus::ExemptionReason
EnumTraits<network::mojom::CookieExemptionReason,
           net::CookieInclusionStatus::ExemptionReason>::
    FromMojom(network::mojom::CookieExemptionReason input) {
  switch (input) {
    case network::mojom::CookieExemptionReason::kNone:
      return net::CookieInclusionStatus::ExemptionReason::kNone;
    case network::mojom::CookieExemptionReason::kUserSetting:
      return net::CookieInclusionStatus::ExemptionReason::kUserSetting;
    case network::mojom::CookieExemptionReason::kEnterprisePolicy:
      return net::CookieInclusionStatus::ExemptionReason::kEnterprisePolicy;
    case network::mojom::CookieExemptionReason::kStorageAccess:
      return net::CookieInclusionStatus::ExemptionReason::kStorageAccess;
    case network::mojom::CookieExemptionReason::kTopLevelStorageAccess:
      return net::CookieInclusionStatus::ExemptionReason::
          kTopLevelStorageAccess;
    case network::mojom::CookieExemptionReason::kScheme:
      return net::CookieInclusionStatus::ExemptionReason::kScheme;
    case network::mojom::CookieExemptionReason::kSameSiteNoneCookiesInSandbox:
      return net::CookieInclusionStatus::ExemptionReason::
          kSameSiteNoneCookiesInSandbox;
  }
  NOTREACHED();
}

bool StructTraits<network::mojom::ExclusionReasonsDataView,
                  net::CookieInclusionStatus::ExclusionReasonBitset>::
    Read(network::mojom::ExclusionReasonsDataView view,
         net::CookieInclusionStatus::ExclusionReasonBitset* out) {
  *out = net::CookieInclusionStatus::ExclusionReasonBitset::FromEnumBitmask(
      view.exclusions_bitmask());
  return view.exclusions_bitmask() == out->ToEnumBitmask();
}

bool StructTraits<network::mojom::WarningReasonsDataView,
                  net::CookieInclusionStatus::WarningReasonBitset>::
    Read(network::mojom::WarningReasonsDataView view,
         net::CookieInclusionStatus::WarningReasonBitset* out) {
  *out = net::CookieInclusionStatus::WarningReasonBitset::FromEnumBitmask(
      view.warnings_bitmask());
  return view.warnings_bitmask() == out->ToEnumBitmask();
}

bool StructTraits<network::mojom::CookieInclusionStatusDataView,
                  net::CookieInclusionStatus>::
    Read(network::mojom::CookieInclusionStatusDataView status,
         net::CookieInclusionStatus* out) {
  net::CookieInclusionStatus::ExemptionReason exemption_reason;
  net::CookieInclusionStatus::ExclusionReasonBitset exclusion_reasons;
  net::CookieInclusionStatus::WarningReasonBitset warning_reasons;
  if (!status.ReadExclusionReasons(&exclusion_reasons) ||
      !status.ReadWarningReasons(&warning_reasons) ||
      !status.ReadExemptionReason(&exemption_reason)) {
    return false;
  }
  std::optional<net::CookieInclusionStatus> maybe_status =
      net::CookieInclusionStatus::MakeFromComponents(
          exclusion_reasons, warning_reasons, exemption_reason);

  if (!maybe_status.has_value()) {
    return false;
  }
  *out = std::move(maybe_status).value();
  return true;
}

}  // namespace mojo

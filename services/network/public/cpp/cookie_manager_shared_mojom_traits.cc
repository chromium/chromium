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
    case net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata:
      return network::mojom::CookieExemptionReason::k3PCDMetadata;
    case net::CookieInclusionStatus::ExemptionReason::k3PCDDeprecationTrial:
      return network::mojom::CookieExemptionReason::k3PCDDeprecationTrial;
    case net::CookieInclusionStatus::ExemptionReason::
        kTopLevel3PCDDeprecationTrial:
      return network::mojom::CookieExemptionReason::
          kTopLevel3PCDDeprecationTrial;
    case net::CookieInclusionStatus::ExemptionReason::k3PCDHeuristics:
      return network::mojom::CookieExemptionReason::k3PCDHeuristics;
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

bool EnumTraits<network::mojom::CookieExemptionReason,
                net::CookieInclusionStatus::ExemptionReason>::
    FromMojom(network::mojom::CookieExemptionReason input,
              net::CookieInclusionStatus::ExemptionReason* output) {
  switch (input) {
    case network::mojom::CookieExemptionReason::kNone:
      *output = net::CookieInclusionStatus::ExemptionReason::kNone;
      return true;
    case network::mojom::CookieExemptionReason::kUserSetting:
      *output = net::CookieInclusionStatus::ExemptionReason::kUserSetting;
      return true;
    case network::mojom::CookieExemptionReason::k3PCDMetadata:
      *output = net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata;
      return true;
    case network::mojom::CookieExemptionReason::k3PCDDeprecationTrial:
      *output =
          net::CookieInclusionStatus::ExemptionReason::k3PCDDeprecationTrial;
      return true;
    case network::mojom::CookieExemptionReason::kTopLevel3PCDDeprecationTrial:
      *output = net::CookieInclusionStatus::ExemptionReason::
          kTopLevel3PCDDeprecationTrial;
      return true;
    case network::mojom::CookieExemptionReason::k3PCDHeuristics:
      *output = net::CookieInclusionStatus::ExemptionReason::k3PCDHeuristics;
      return true;
    case network::mojom::CookieExemptionReason::kEnterprisePolicy:
      *output = net::CookieInclusionStatus::ExemptionReason::kEnterprisePolicy;
      return true;
    case network::mojom::CookieExemptionReason::kStorageAccess:
      *output = net::CookieInclusionStatus::ExemptionReason::kStorageAccess;
      return true;
    case network::mojom::CookieExemptionReason::kTopLevelStorageAccess:
      *output =
          net::CookieInclusionStatus::ExemptionReason::kTopLevelStorageAccess;
      return true;
    case network::mojom::CookieExemptionReason::kScheme:
      *output = net::CookieInclusionStatus::ExemptionReason::kScheme;
      return true;
    case network::mojom::CookieExemptionReason::kSameSiteNoneCookiesInSandbox:
      *output = net::CookieInclusionStatus::ExemptionReason::
          kSameSiteNoneCookiesInSandbox;
      return true;
  }
  return false;
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

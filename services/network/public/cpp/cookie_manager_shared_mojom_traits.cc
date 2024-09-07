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
  }
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
  }
}

bool StructTraits<network::mojom::CookieInclusionStatusDataView,
                  net::CookieInclusionStatus>::
    Read(network::mojom::CookieInclusionStatusDataView status,
         net::CookieInclusionStatus* out) {
  net::CookieInclusionStatus::ExemptionReason exemption_reason;

  out->set_exclusion_reasons(status.exclusion_reasons());
  out->set_warning_reasons(status.warning_reasons());
  if (!status.ReadExemptionReason(&exemption_reason)) {
    return false;
  }
  out->MaybeSetExemptionReason(exemption_reason);

  return net::CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      status.exclusion_reasons(), status.warning_reasons());
}

}  // namespace mojo

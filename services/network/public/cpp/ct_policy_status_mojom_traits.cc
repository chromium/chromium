// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ct_policy_status_mojom_traits.h"

#include "base/notreached.h"
#include "net/cert/ct_policy_status.h"
#include "services/network/public/mojom/ct_policy_status.mojom-shared.h"

namespace mojo {

// static
network::mojom::CTPolicyCompliance EnumTraits<
    network::mojom::CTPolicyCompliance,
    net::ct::CTPolicyCompliance>::ToMojom(net::ct::CTPolicyCompliance status) {
  switch (status) {
    case net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS:
      return network::mojom::CTPolicyCompliance::kCtPolicyCompliesViaScts;
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS:
      return network::mojom::CTPolicyCompliance::kCtPolicyNotEnoughScts;
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS:
      return network::mojom::CTPolicyCompliance::kCtPolicyNotDiverseScts;
    case net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY:
      return network::mojom::CTPolicyCompliance::kCtPolicyBuildNotTimely;
    case net::ct::CTPolicyCompliance::
        CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE:
      return network::mojom::CTPolicyCompliance::
          kCtPolicyComplianceDetailsNotAvailable;
    case net::ct::CTPolicyCompliance::CT_POLICY_COUNT:
      // This is a placeholder for UMA and should never be passed over IPC.
      NOTREACHED();
  }
  NOTREACHED();
}

// static
net::ct::CTPolicyCompliance
EnumTraits<network::mojom::CTPolicyCompliance, net::ct::CTPolicyCompliance>::
    FromMojom(network::mojom::CTPolicyCompliance input) {
  switch (input) {
    case network::mojom::CTPolicyCompliance::kCtPolicyCompliesViaScts:
      return net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    case network::mojom::CTPolicyCompliance::kCtPolicyNotEnoughScts:
      return net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
    case network::mojom::CTPolicyCompliance::kCtPolicyNotDiverseScts:
      return net::ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
    case network::mojom::CTPolicyCompliance::kCtPolicyBuildNotTimely:
      return net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY;
    case network::mojom::CTPolicyCompliance::
        kCtPolicyComplianceDetailsNotAvailable:
      return net::ct::CTPolicyCompliance::
          CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE;
  }
  NOTREACHED();
}

// static
network::mojom::CTRequirementsStatus
EnumTraits<network::mojom::CTRequirementsStatus,
           net::ct::CTRequirementsStatus>::ToMojom(net::ct::CTRequirementsStatus
                                                       status) {
  switch (status) {
    case net::ct::CTRequirementsStatus::CT_NOT_REQUIRED:
      return network::mojom::CTRequirementsStatus::kNotRequired;
    case net::ct::CTRequirementsStatus::CT_REQUIREMENTS_MET:
      return network::mojom::CTRequirementsStatus::kRequirementsMet;
    case net::ct::CTRequirementsStatus::CT_REQUIREMENTS_NOT_MET:
      return network::mojom::CTRequirementsStatus::kRequirementsNotMet;
    case net::ct::CTRequirementsStatus::CT_REQUIREMENT_OVERRIDDEN:
      return network::mojom::CTRequirementsStatus::kRequirementOverridden;
    case net::ct::CTRequirementsStatus::
        CT_REQUIREMENT_OVERRIDDEN_APPLIES_ACROSS_NAMES:
      return network::mojom::CTRequirementsStatus::
          kRequirementOverriddenAppliesAcrossNames;
  }
  NOTREACHED();
}

// static
net::ct::CTRequirementsStatus EnumTraits<network::mojom::CTRequirementsStatus,
                                         net::ct::CTRequirementsStatus>::
    FromMojom(network::mojom::CTRequirementsStatus input) {
  switch (input) {
    case network::mojom::CTRequirementsStatus::kNotRequired:
      return net::ct::CTRequirementsStatus::CT_NOT_REQUIRED;
    case network::mojom::CTRequirementsStatus::kRequirementsMet:
      return net::ct::CTRequirementsStatus::CT_REQUIREMENTS_MET;
    case network::mojom::CTRequirementsStatus::kRequirementsNotMet:
      return net::ct::CTRequirementsStatus::CT_REQUIREMENTS_NOT_MET;
    case network::mojom::CTRequirementsStatus::kRequirementOverridden:
      return net::ct::CTRequirementsStatus::CT_REQUIREMENT_OVERRIDDEN;
    case network::mojom::CTRequirementsStatus::
        kRequirementOverriddenAppliesAcrossNames:
      return net::ct::CTRequirementsStatus::
          CT_REQUIREMENT_OVERRIDDEN_APPLIES_ACROSS_NAMES;
  }
  NOTREACHED();
}

}  // namespace mojo

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/require_ct_delegate.h"

namespace net {

// static
ct::CTRequirementsStatus RequireCTDelegate::CheckCTRequirements(
    const RequireCTDelegate* delegate,
    std::string_view host,
    bool is_issued_by_known_root,
    const std::vector<SHA256HashValue>& public_key_hashes,
    const X509Certificate* validated_certificate_chain,
    ct::CTPolicyCompliance policy_compliance) {
  // CT is not required if the certificate does not chain to a publicly
  // trusted root certificate.
  if (!is_issued_by_known_root) {
    return ct::CTRequirementsStatus::CT_NOT_REQUIRED;
  }

  // A connection is considered compliant if it has sufficient SCTs or if the
  // build is outdated. Other statuses are not considered compliant; this
  // includes COMPLIANCE_DETAILS_NOT_AVAILABLE because compliance must have been
  // evaluated in order to determine that the connection is compliant.
  bool complies =
      (policy_compliance ==
           ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS ||
       policy_compliance == ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY);

  CTRequirementLevel ct_required = CTRequirementLevel::NOT_REQUIRED;
  if (delegate) {
    // Allow the delegate to override the CT requirement state.
    ct_required = delegate->IsCTRequiredForHost(
        host, validated_certificate_chain, public_key_hashes);
  }
  switch (ct_required) {
    case CTRequirementLevel::REQUIRED:
      return complies ? ct::CTRequirementsStatus::CT_REQUIREMENTS_MET
                      : ct::CTRequirementsStatus::CT_REQUIREMENTS_NOT_MET;
    case CTRequirementLevel::NOT_REQUIRED:
      return ct::CTRequirementsStatus::CT_NOT_REQUIRED;
  }
}

}  // namespace net

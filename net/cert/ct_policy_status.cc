// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_policy_status.h"

#include "base/notreached.h"

namespace net::ct {

const char* CTPolicyComplianceToString(CTPolicyCompliance status) {
  switch (status) {
    case CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS:
      return "COMPLIES_VIA_SCTS";
    case CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS:
      return "NOT_ENOUGH_SCTS";
    case CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS:
      return "NOT_DIVERSE_SCTS";
    case CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY:
      return "BUILD_NOT_TIMELY";
    case CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE:
      return "COMPLIANCE_DETAILS_NOT_AVAILABLE";
    case CTPolicyCompliance::CT_POLICY_COUNT:
      NOTREACHED();
  }

  NOTREACHED();
}

const char* CTRequirementStatusToString(CTRequirementsStatus status) {
  switch (status) {
    case CTRequirementsStatus::CT_NOT_REQUIRED:
      return "CT_NOT_REQUIRED";
    case CTRequirementsStatus::CT_REQUIREMENTS_MET:
      return "CT_REQUIREMENTS_MET";
    case CTRequirementsStatus::CT_REQUIREMENTS_NOT_MET:
      return "CT_REQUIREMENTS_NOT_MET";
  }

  NOTREACHED();
}

}  // namespace net::ct

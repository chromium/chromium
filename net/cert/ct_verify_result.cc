// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_verify_result.h"

#include "net/cert/ct_policy_status.h"

namespace net {

namespace ct {

CTVerifyResult::CTVerifyResult()
    : policy_compliance(
          ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE),
      policy_compliance_required(false) {}

CTVerifyResult::CTVerifyResult(const CTVerifyResult& other) = default;

CTVerifyResult::~CTVerifyResult() = default;

SCTList SCTsMatchingStatus(
    const SignedCertificateTimestampAndStatusList& sct_and_status_list,
    SCTVerifyStatus match_status) {
  SCTList result;
  for (const auto& sct_and_status : sct_and_status_list)
    if (sct_and_status.status == match_status)
      result.push_back(sct_and_status.sct);

  return result;
}

}  // namespace ct

}  // namespace net

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_VERIFY_RESULT_H_
#define NET_CERT_CT_VERIFY_RESULT_H_

#include <vector>

#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"

namespace net {

namespace ct {

enum class CTPolicyCompliance;

// Holds Signed Certificate Timestamps, depending on their verification
// results, and information about CT policies that were applied on the
// connection.
struct NET_EXPORT CTVerifyResult {
  CTVerifyResult();
  CTVerifyResult(const CTVerifyResult& other);
  ~CTVerifyResult();

  // All SCTs and their statuses
  SignedCertificateTimestampAndStatusList scts;

  // The result of evaluating whether the connection complies with the
  // CT certificate policy.
  CTPolicyCompliance policy_compliance;
  // True if the connection was required to comply with the CT certificate
  // policy. This value is not meaningful if |policy_compliance| is
  // COMPLIANCE_DETAILS_NOT_AVAILABLE.
  bool policy_compliance_required;
};

// Returns a list of SCTs from |sct_and_status_list| whose status matches
// |match_status|.
SCTList NET_EXPORT SCTsMatchingStatus(
    const SignedCertificateTimestampAndStatusList& sct_and_status_list,
    SCTVerifyStatus match_status);

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_VERIFY_RESULT_H_

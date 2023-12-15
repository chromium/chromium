// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_POLICY_STATUS_H_
#define NET_CERT_CT_POLICY_STATUS_H_

#include "net/base/net_export.h"

namespace net::ct {

// Information about the connection's compliance with the CT policy. This value
// is histogrammed, so do not re-order or change values, and add new values at
// the end.
enum class CTPolicyCompliance {
  // The connection complied with the certificate policy by
  // including SCTs that satisfy the policy.
  CT_POLICY_COMPLIES_VIA_SCTS = 0,
  // The connection did not have enough SCTs to comply.
  CT_POLICY_NOT_ENOUGH_SCTS = 1,
  // The connection did not have diverse enough SCTs to comply.
  CT_POLICY_NOT_DIVERSE_SCTS = 2,
  // The connection cannot be considered compliant because the build
  // isn't timely and therefore log information might be out of date
  // (for example a log might no longer be considered trustworthy).
  CT_POLICY_BUILD_NOT_TIMELY = 3,
  // Compliance details for the connection are not available, e.g. because a
  // resource was loaded from disk cache.
  CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE = 4,
  CT_POLICY_COUNT
};

NET_EXPORT const char* CTPolicyComplianceToString(CTPolicyCompliance status);

}  // namespace net::ct

#endif  // NET_CERT_CT_POLICY_STATUS_H_

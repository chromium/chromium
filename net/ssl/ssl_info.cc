// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_info.h"

#include "net/cert/x509_certificate.h"

namespace net {

SSLInfo::SSLInfo() = default;

SSLInfo::SSLInfo(const SSLInfo& info) = default;

SSLInfo::~SSLInfo() = default;

SSLInfo& SSLInfo::operator=(const SSLInfo& info) = default;

void SSLInfo::Reset() {
  *this = SSLInfo();
}

void SSLInfo::UpdateCertificateTransparencyInfo(
    const ct::CTVerifyResult& ct_verify_result) {
  signed_certificate_timestamps.insert(signed_certificate_timestamps.end(),
                                       ct_verify_result.scts.begin(),
                                       ct_verify_result.scts.end());

  ct_policy_compliance = ct_verify_result.policy_compliance;
  ct_policy_compliance_required = ct_verify_result.policy_compliance_required;
}

}  // namespace net

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_result.h"

#include <tuple>

#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_signed_certificate_timestamp_log_param.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_net_log_param.h"

namespace net {

CertVerifyResult::CertVerifyResult() {
  Reset();
}

CertVerifyResult::CertVerifyResult(const CertVerifyResult& other) {
  *this = other;
}

CertVerifyResult::~CertVerifyResult() = default;

void CertVerifyResult::Reset() {
  verified_cert = nullptr;
  cert_status = 0;
  has_sha1 = false;
  is_issued_by_known_root = false;
  is_issued_by_additional_trust_anchor = false;

  public_key_hashes.clear();
  ocsp_result = bssl::OCSPVerifyResult();

  scts.clear();
  policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE;
}

base::Value::Dict CertVerifyResult::NetLogParams(int net_error) const {
  base::Value::Dict dict;
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error < 0)
    dict.Set("net_error", net_error);
  dict.Set("is_issued_by_known_root", is_issued_by_known_root);
  if (is_issued_by_additional_trust_anchor) {
    dict.Set("is_issued_by_additional_trust_anchor", true);
  }
  dict.Set("cert_status", static_cast<int>(cert_status));
  // TODO(mattm): This double-wrapping of the certificate list is weird. Remove
  // this (probably requires updates to netlog-viewer).
  base::Value::Dict certificate_dict;
  certificate_dict.Set("certificates",
                       net::NetLogX509CertificateList(verified_cert.get()));
  dict.Set("verified_cert", std::move(certificate_dict));

  base::Value::List hashes;
  for (const auto& public_key_hash : public_key_hashes)
    hashes.Append(public_key_hash.ToString());
  dict.Set("public_key_hashes", std::move(hashes));

  dict.Set("scts", net::NetLogSignedCertificateTimestampParams(&scts));
  dict.Set("ct_compliance_status",
           CTPolicyComplianceToString(policy_compliance));

  return dict;
}

}  // namespace net

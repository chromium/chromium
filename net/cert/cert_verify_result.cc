// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_result.h"

#include <tuple>

#include "base/values.h"
#include "net/base/net_errors.h"
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

CertVerifyResult& CertVerifyResult::operator=(const CertVerifyResult& other) {
  verified_cert = other.verified_cert;
  cert_status = other.cert_status;
  has_md2 = other.has_md2;
  has_md4 = other.has_md4;
  has_md5 = other.has_md5;
  has_sha1 = other.has_sha1;
  has_sha1_leaf = other.has_sha1_leaf;
  is_issued_by_known_root = other.is_issued_by_known_root;
  is_issued_by_additional_trust_anchor =
      other.is_issued_by_additional_trust_anchor;

  public_key_hashes = other.public_key_hashes;
  ocsp_result = other.ocsp_result;

  ClearAllUserData();
  CloneDataFrom(other);

  return *this;
}

void CertVerifyResult::Reset() {
  verified_cert = nullptr;
  cert_status = 0;
  has_md2 = false;
  has_md4 = false;
  has_md5 = false;
  has_sha1 = false;
  has_sha1_leaf = false;
  is_issued_by_known_root = false;
  is_issued_by_additional_trust_anchor = false;

  public_key_hashes.clear();
  ocsp_result = OCSPVerifyResult();

  ClearAllUserData();
}

base::Value CertVerifyResult::NetLogParams(int net_error) const {
  base::DictionaryValue results;
  DCHECK_NE(ERR_IO_PENDING, net_error);
  if (net_error < 0)
    results.SetIntKey("net_error", net_error);
  if (has_md5)
    results.SetBoolKey("has_md5", true);
  if (has_md2)
    results.SetBoolKey("has_md2", true);
  if (has_md4)
    results.SetBoolKey("has_md4", true);
  results.SetBoolKey("is_issued_by_known_root", is_issued_by_known_root);
  if (is_issued_by_additional_trust_anchor) {
    results.SetBoolKey("is_issued_by_additional_trust_anchor", true);
  }
  results.SetIntKey("cert_status", cert_status);
  // TODO(mattm): This double-wrapping of the certificate list is weird. Remove
  // this (probably requires updates to netlog-viewer).
  base::Value certificate_dict(base::Value::Type::DICTIONARY);
  certificate_dict.SetKey("certificates",
                          net::NetLogX509CertificateList(verified_cert.get()));
  results.SetKey("verified_cert", std::move(certificate_dict));

  base::Value hashes(base::Value::Type::LIST);
  for (const auto& public_key_hash : public_key_hashes)
    hashes.Append(public_key_hash.ToString());
  results.SetKey("public_key_hashes", std::move(hashes));

  return std::move(results);
}

}  // namespace net

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_result.h"

#include <tuple>

#include "net/cert/x509_certificate.h"

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

bool CertVerifyResult::operator==(const CertVerifyResult& other) const {
  return (!!verified_cert == !!other.verified_cert) &&
         (!verified_cert ||
          verified_cert->EqualsIncludingChain(other.verified_cert.get())) &&
         std::tie(cert_status, has_md2, has_md4, has_md5, has_sha1,
                  has_sha1_leaf, public_key_hashes, is_issued_by_known_root,
                  is_issued_by_additional_trust_anchor, ocsp_result) ==
             std::tie(other.cert_status, other.has_md2, other.has_md4,
                      other.has_md5, other.has_sha1, other.has_sha1_leaf,
                      other.public_key_hashes, other.is_issued_by_known_root,
                      other.is_issued_by_additional_trust_anchor,
                      other.ocsp_result);
}

}  // namespace net

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/security/cert_host_pair.h"

#include <utility>

#include "net/cert/x509_certificate.h"

namespace web {

CertHostPair::CertHostPair(scoped_refptr<net::X509Certificate> cert,
                           std::string host)
    : cert_(std::move(cert)),
      host_(std::move(host)),
      cert_hash_(cert_->CalculateChainFingerprint256()) {}

CertHostPair::CertHostPair(const CertHostPair& other) = default;

CertHostPair::~CertHostPair() {}

bool CertHostPair::operator<(const CertHostPair& other) const {
  return std::tie(host_, cert_hash_) < std::tie(other.host_, other.cert_hash_);
}

}  // namespace web

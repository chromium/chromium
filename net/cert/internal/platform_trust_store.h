// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_PLATFORM_TRUST_STORE_H_
#define NET_CERT_INTERNAL_PLATFORM_TRUST_STORE_H_

#include <vector>

#include "net/base/net_export.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

// Extension of bssl::TrustStore that supports enumerating all
// user added certs.
class NET_EXPORT PlatformTrustStore : public bssl::TrustStore {
 public:
  PlatformTrustStore() = default;

  PlatformTrustStore(const PlatformTrustStore&) = delete;
  PlatformTrustStore& operator=(const PlatformTrustStore&) = delete;

  struct NET_EXPORT CertWithTrust {
    CertWithTrust(std::vector<uint8_t> cert_bytes,
                  bssl::CertificateTrust trust);
    ~CertWithTrust();
    CertWithTrust(const CertWithTrust&);
    CertWithTrust& operator=(const CertWithTrust& other);
    CertWithTrust(CertWithTrust&&);
    CertWithTrust& operator=(CertWithTrust&& other);

    std::vector<uint8_t> cert_bytes;
    bssl::CertificateTrust trust;
  };

  virtual std::vector<CertWithTrust> GetAllUserAddedCerts() = 0;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_PLATFORM_TRUST_STORE_H_

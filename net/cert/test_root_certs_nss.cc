// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include <cert.h>

#include "base/logging.h"
#include "base/macros.h"
#include "crypto/nss_util.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

TestRootCerts::TrustEntry::TrustEntry(ScopedCERTCertificate certificate,
                                      const CERTCertTrust& trust)
    : certificate_(std::move(certificate)), trust_(trust) {}

TestRootCerts::TrustEntry::~TrustEntry() = default;

bool TestRootCerts::Add(X509Certificate* certificate) {
  ScopedCERTCertificate cert_handle =
      x509_util::CreateCERTCertificateFromX509Certificate(certificate);
  if (!cert_handle)
    return false;
  // Preserve the original trust bits so that they can be restored when
  // the certificate is removed.
  CERTCertTrust original_trust;
  SECStatus rv = CERT_GetCertTrust(cert_handle.get(), &original_trust);
  if (rv != SECSuccess) {
    // CERT_GetCertTrust will fail if the certificate does not have any
    // particular trust settings associated with it, and attempts to use
    // |original_trust| later to restore the original trust settings will not
    // cause the trust settings to be revoked. If the certificate has no
    // particular trust settings associated with it, mark the certificate as
    // a valid CA certificate with no specific trust.
    rv = CERT_DecodeTrustString(&original_trust, "c,c,c");
  }

  // Change the trust bits to unconditionally trust this certificate.
  CERTCertTrust new_trust;
  rv = CERT_DecodeTrustString(&new_trust, "TCPu,Cu,Tu");
  if (rv != SECSuccess) {
    LOG(ERROR) << "Cannot decode certificate trust string.";
    return false;
  }

  rv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert_handle.get(),
                            &new_trust);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Cannot change certificate trust.";
    return false;
  }

  trust_cache_.push_back(
      std::make_unique<TrustEntry>(std::move(cert_handle), original_trust));

  // Add the certificate to the parallel |test_trust_store_|.  TrustStoreNSS
  // ignores temporary certs, so it won't see the cert that was added above.
  // (See https://crbug.com/951166)
  // TODO(https://crbug.com/951479): remove this when the istemp check is
  // removed from TrustStoreNSS.
  CertErrors errors;
  scoped_refptr<ParsedCertificate> parsed = ParsedCertificate::Create(
      bssl::UpRef(certificate->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &errors);
  if (!parsed)
    return false;
  test_trust_store_.AddTrustAnchor(parsed);

  return true;
}

void TestRootCerts::Clear() {
  // Restore the certificate trusts to what they were originally, before
  // Add() was called. Work from the rear first, since if a certificate was
  // added twice, the second entry's original trust status will be that of
  // the first entry, while the first entry contains the desired resultant
  // status.
  for (auto it = trust_cache_.rbegin(); it != trust_cache_.rend(); ++it) {
    CERTCertTrust original_trust = (*it)->trust();
    SECStatus rv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(),
                                        (*it)->certificate(),
                                        &original_trust);
    // DCHECK(), rather than LOG(), as a failure to restore the original
    // trust can cause flake or hard-to-trace errors in any unit tests that
    // occur after Clear() has been called.
    DCHECK_EQ(SECSuccess, rv) << "Cannot restore certificate trust.";
  }
  trust_cache_.clear();

  test_trust_store_.Clear();
}

bool TestRootCerts::IsEmpty() const {
  return trust_cache_.empty();
}

bool TestRootCerts::Contains(CERTCertificate* cert) const {
  for (const auto& item : trust_cache_)
    if (x509_util::IsSameCertificate(cert, item->certificate()))
      return true;

  return false;
}

TestRootCerts::~TestRootCerts() {
  Clear();
}

void TestRootCerts::Init() {
  crypto::EnsureNSSInit();
}

}  // namespace net

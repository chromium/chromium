// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_win.h"

#include <string_view>

#include "base/logging.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace x509_util {

base::span<const uint8_t> CertContextAsSpan(PCCERT_CONTEXT os_cert) {
  // SAFETY: `os_cert` is a pointer to a CERT_CONTEXT which contains a pointer
  // to the certificate DER encoded data in `pbCertEncoded` of length
  // `cbCertEncoded`.
  return UNSAFE_BUFFERS(
      base::make_span(os_cert->pbCertEncoded, os_cert->cbCertEncoded));
}

scoped_refptr<X509Certificate> CreateX509CertificateFromCertContexts(
    PCCERT_CONTEXT os_cert,
    const std::vector<PCCERT_CONTEXT>& os_chain) {
  return CreateX509CertificateFromCertContexts(os_cert, os_chain, {});
}

scoped_refptr<X509Certificate> CreateX509CertificateFromCertContexts(
    PCCERT_CONTEXT os_cert,
    const std::vector<PCCERT_CONTEXT>& os_chain,
    X509Certificate::UnsafeCreateOptions options) {
  if (!os_cert || !os_cert->pbCertEncoded || !os_cert->cbCertEncoded)
    return nullptr;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(
      x509_util::CreateCryptoBuffer(CertContextAsSpan(os_cert)));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (PCCERT_CONTEXT os_intermediate : os_chain) {
    if (!os_intermediate || !os_intermediate->pbCertEncoded ||
        !os_intermediate->cbCertEncoded)
      return nullptr;
    intermediates.push_back(
        x509_util::CreateCryptoBuffer(CertContextAsSpan(os_intermediate)));
  }

  return X509Certificate::CreateFromBufferUnsafeOptions(
      std::move(cert_handle), std::move(intermediates), options);
}

crypto::ScopedPCCERT_CONTEXT CreateCertContextWithChain(
    const X509Certificate* cert) {
  return CreateCertContextWithChain(cert, InvalidIntermediateBehavior::kFail);
}

crypto::ScopedPCCERT_CONTEXT CreateCertContextWithChain(
    const X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior) {
  // Create an in-memory certificate store to hold the certificate and its
  // intermediate certificates. The store will be referenced in the returned
  // PCCERT_CONTEXT, and will not be freed until the PCCERT_CONTEXT is freed.
  crypto::ScopedHCERTSTORE store(
      CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL,
                    CERT_STORE_DEFER_CLOSE_UNTIL_LAST_FREE_FLAG, nullptr));
  if (!store.is_valid())
    return nullptr;

  PCCERT_CONTEXT primary_cert = nullptr;

  BOOL ok = CertAddEncodedCertificateToStore(
      store.get(), X509_ASN_ENCODING, CRYPTO_BUFFER_data(cert->cert_buffer()),
      base::checked_cast<DWORD>(CRYPTO_BUFFER_len(cert->cert_buffer())),
      CERT_STORE_ADD_ALWAYS, &primary_cert);
  if (!ok || !primary_cert)
    return nullptr;
  crypto::ScopedPCCERT_CONTEXT scoped_primary_cert(primary_cert);

  for (const auto& intermediate : cert->intermediate_buffers()) {
    ok = CertAddEncodedCertificateToStore(
        store.get(), X509_ASN_ENCODING, CRYPTO_BUFFER_data(intermediate.get()),
        base::checked_cast<DWORD>(CRYPTO_BUFFER_len(intermediate.get())),
        CERT_STORE_ADD_ALWAYS, nullptr);
    if (!ok) {
      if (invalid_intermediate_behavior == InvalidIntermediateBehavior::kFail)
        return nullptr;
      LOG(WARNING) << "error parsing intermediate";
    }
  }

  // Note: |primary_cert| retains a reference to |store|, so the store will
  // actually be freed when |primary_cert| is freed.
  return scoped_primary_cert;
}

SHA256HashValue CalculateFingerprint256(PCCERT_CONTEXT cert) {
  DCHECK(nullptr != cert->pbCertEncoded);
  DCHECK_NE(0u, cert->cbCertEncoded);

  SHA256HashValue sha256;

  // Use crypto::SHA256HashString for two reasons:
  // * < Windows Vista does not have universal SHA-256 support.
  // * More efficient on Windows > Vista (less overhead since non-default CSP
  // is not needed).
  crypto::SHA256HashString(base::as_string_view(CertContextAsSpan(cert)),
                           sha256.data, sizeof(sha256.data));
  return sha256;
}

bool IsSelfSigned(PCCERT_CONTEXT cert_handle) {
  bool valid_signature = !!CryptVerifyCertificateSignatureEx(
      NULL, X509_ASN_ENCODING, CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,
      reinterpret_cast<void*>(const_cast<PCERT_CONTEXT>(cert_handle)),
      CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,
      reinterpret_cast<void*>(const_cast<PCERT_CONTEXT>(cert_handle)), 0,
      nullptr);
  if (!valid_signature)
    return false;
  return !!CertCompareCertificateName(X509_ASN_ENCODING,
                                      &cert_handle->pCertInfo->Subject,
                                      &cert_handle->pCertInfo->Issuer);
}

}  // namespace x509_util

}  // namespace net

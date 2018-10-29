// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_WIN_H_
#define NET_CERT_X509_UTIL_WIN_H_

#include <memory>
#include <vector>

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "base/win/wincrypt_shim.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net {

struct FreeCertContextFunctor {
  void operator()(PCCERT_CONTEXT context) const {
    if (context)
      CertFreeCertificateContext(context);
  }
};

using ScopedPCCERT_CONTEXT =
    std::unique_ptr<const CERT_CONTEXT, FreeCertContextFunctor>;

namespace x509_util {

// Creates an X509Certificate representing |os_cert| with intermediates
// |os_chain|.
NET_EXPORT scoped_refptr<X509Certificate> CreateX509CertificateFromCertContexts(
    PCCERT_CONTEXT os_cert,
    const std::vector<PCCERT_CONTEXT>& os_chain);
// Creates an X509Certificate with non-standard parsing options.
// Do not use without consulting //net owners.
NET_EXPORT scoped_refptr<X509Certificate> CreateX509CertificateFromCertContexts(
    PCCERT_CONTEXT os_cert,
    const std::vector<PCCERT_CONTEXT>& os_chain,
    X509Certificate::UnsafeCreateOptions options);

// Returns a new PCCERT_CONTEXT containing the certificate and its
// intermediate certificates, or NULL on failure. This function is only
// necessary if the CERT_CONTEXT.hCertStore member will be accessed or
// enumerated, which is generally true for any CryptoAPI functions involving
// certificate chains, including validation or certificate display.
//
// While the returned PCCERT_CONTEXT and its HCERTSTORE can safely be used on
// multiple threads if no further modifications happen, it is generally
// preferable for each thread that needs such a context to obtain its own,
// rather than risk thread-safety issues by sharing.
NET_EXPORT ScopedPCCERT_CONTEXT
CreateCertContextWithChain(const X509Certificate* cert);

// Specify behavior if an intermediate certificate fails CERT_CONTEXT parsing.
// kFail means the function should return a failure result immediately. kIgnore
// means the invalid intermediate is not added to the output context.
enum class InvalidIntermediateBehavior { kFail, kIgnore };

// As CreateCertContextWithChain above, but |invalid_intermediate_behavior|
// specifies behavior if intermediates of |cert| could not be converted.
NET_EXPORT ScopedPCCERT_CONTEXT CreateCertContextWithChain(
    const X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior);

// Calculates the SHA-256 fingerprint of the certificate.  Returns an empty
// (all zero) fingerprint on failure.
NET_EXPORT SHA256HashValue CalculateFingerprint256(PCCERT_CONTEXT cert);

// Returns true if the certificate is self-signed.
NET_EXPORT bool IsSelfSigned(PCCERT_CONTEXT cert_handle);

}  // namespace x509_util

}  // namespace net

#endif  // NET_CERT_X509_UTIL_WIN_H_

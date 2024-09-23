// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_WIN_H_
#define NET_CERT_X509_UTIL_WIN_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net::x509_util {

// Returns a span containing the DER encoded certificate data for `os_cert`.
NET_EXPORT base::span<const uint8_t> CertContextAsSpan(PCCERT_CONTEXT os_cert);

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
NET_EXPORT crypto::ScopedPCCERT_CONTEXT CreateCertContextWithChain(
    const X509Certificate* cert);

// Specify behavior if an intermediate certificate fails CERT_CONTEXT parsing.
// kFail means the function should return a failure result immediately. kIgnore
// means the invalid intermediate is not added to the output context.
enum class InvalidIntermediateBehavior { kFail, kIgnore };

// As CreateCertContextWithChain above, but |invalid_intermediate_behavior|
// specifies behavior if intermediates of |cert| could not be converted.
NET_EXPORT crypto::ScopedPCCERT_CONTEXT CreateCertContextWithChain(
    const X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior);

// Calculates the SHA-256 fingerprint of the certificate.  Returns an empty
// (all zero) fingerprint on failure.
NET_EXPORT SHA256HashValue CalculateFingerprint256(PCCERT_CONTEXT cert);

// Returns true if the certificate is self-signed.
NET_EXPORT bool IsSelfSigned(PCCERT_CONTEXT cert_handle);

}  // namespace net::x509_util

#endif  // NET_CERT_X509_UTIL_WIN_H_

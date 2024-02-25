// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_APPLE_H_
#define NET_CERT_X509_UTIL_APPLE_H_

#include <CoreFoundation/CFArray.h>
#include <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net {
namespace x509_util {

// Creates a SecCertificate handle from the DER-encoded representation.
// Returns NULL on failure.
NET_EXPORT base::apple::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromBytes(base::span<const uint8_t> data);

// Returns a SecCertificate representing |cert|, or NULL on failure.
NET_EXPORT base::apple::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromX509Certificate(const X509Certificate* cert);

// Returns a new CFMutableArrayRef containing this certificate and its
// intermediate certificates in the form expected by Security.framework
// and Keychain Services, or NULL on failure.
// The first item in the array will be this certificate, followed by its
// intermediates, if any.
NET_EXPORT base::apple::ScopedCFTypeRef<CFMutableArrayRef>
CreateSecCertificateArrayForX509Certificate(X509Certificate* cert);

// Specify behavior if an intermediate certificate fails SecCertificate
// parsing. kFail means the function should return a failure result
// immediately. kIgnore means the invalid intermediate is not added to the
// output container.
enum class InvalidIntermediateBehavior { kFail, kIgnore };

// Returns a new CFMutableArrayRef containing this certificate and its
// intermediate certificates in the form expected by Security.framework
// and Keychain Services. Returns NULL if the certificate could not be
// converted. |invalid_intermediate_behavior| specifies behavior if
// intermediates of |cert| could not be converted.
NET_EXPORT base::apple::ScopedCFTypeRef<CFMutableArrayRef>
CreateSecCertificateArrayForX509Certificate(
    X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior);

// Creates an X509Certificate representing |sec_cert| with intermediates
// |sec_chain|.
NET_EXPORT scoped_refptr<X509Certificate>
CreateX509CertificateFromSecCertificate(
    base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::apple::ScopedCFTypeRef<SecCertificateRef>>&
        sec_chain);

// Creates an X509Certificate with non-standard parsing options.
// Do not use without consulting //net owners.
NET_EXPORT scoped_refptr<X509Certificate>
CreateX509CertificateFromSecCertificate(
    base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::apple::ScopedCFTypeRef<SecCertificateRef>>&
        sec_chain,
    X509Certificate::UnsafeCreateOptions options);

// Calculates the SHA-256 fingerprint of the certificate.  Returns an empty
// (all zero) fingerprint on failure.
NET_EXPORT SHA256HashValue CalculateFingerprint256(SecCertificateRef cert);

// Returns a new CFArrayRef containing the certificate chain built in |trust|.
base::apple::ScopedCFTypeRef<CFArrayRef> CertificateChainFromSecTrust(
    SecTrustRef trust);

}  // namespace x509_util
}  // namespace net

#endif  // NET_CERT_X509_UTIL_APPLE_H_

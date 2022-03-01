// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_IOS_AND_MAC_H_
#define NET_CERT_X509_UTIL_IOS_AND_MAC_H_

#include <CoreFoundation/CFArray.h>
#include <Security/Security.h>

#include "base/mac/scoped_cftyperef.h"
#include "net/base/net_export.h"

namespace net {

class X509Certificate;

namespace x509_util {

// Creates a SecCertificate handle from the DER-encoded representation.
// Returns NULL on failure.
NET_EXPORT base::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromBytes(const uint8_t* data, size_t length);

// Returns a SecCertificate representing |cert|, or NULL on failure.
NET_EXPORT base::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromX509Certificate(const X509Certificate* cert);

// Returns a new CFMutableArrayRef containing this certificate and its
// intermediate certificates in the form expected by Security.framework
// and Keychain Services, or NULL on failure.
// The first item in the array will be this certificate, followed by its
// intermediates, if any.
NET_EXPORT base::ScopedCFTypeRef<CFMutableArrayRef>
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
NET_EXPORT base::ScopedCFTypeRef<CFMutableArrayRef>
CreateSecCertificateArrayForX509Certificate(
    X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior);

}  // namespace x509_util

}  // namespace net

#endif  // NET_CERT_X509_UTIL_IOS_AND_MAC_H_

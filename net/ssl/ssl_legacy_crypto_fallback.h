// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_LEGACY_CRYPTO_FALLBACK_H_
#define NET_SSL_SSL_LEGACY_CRYPTO_FALLBACK_H_

namespace net {

// Classifies reasons why a connection might require the legacy crypto fallback.
// Note that, although SHA-1 certificates are no longer accepted, servers may
// still send unused certificates. Some such servers additionally match their
// certificate chains against the ClientHello. These servers require the client
// advertise legacy algorithms despite not actually using them.
//
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "SSLLegacyCryptoFallback" in src/tools/metrics/histograms/enums.xml.
enum class SSLLegacyCryptoFallback {
  // The connection did not use the fallback.
  kNoFallback = 0,
  // No longer used.
  //   kUsed3DES = 1,
  // The connection used the fallback and negotiated SHA-1.
  kUsedSHA1 = 2,
  // The connection used the fallback and sent a certificate signed with
  // RSASSA-PKCS1-v1_5-SHA-1.
  kSentSHA1Cert = 3,
  // No longer used.
  //   kSentSHA1CertAndUsed3DES = 4,
  // The connection used the fallback, negotiated SHA-1, and sent a certificate
  // signed with RSASSA-PKCS1-v1_5-SHA-1.
  kSentSHA1CertAndUsedSHA1 = 5,
  // The connection used the fallback for an unknown reason, likely a
  // transient network error.
  kUnknownReason = 6,
  kMaxValue = kUnknownReason,
};

}  // namespace net

#endif  // NET_SSL_SSL_LEGACY_CRYPTO_FALLBACK_H_

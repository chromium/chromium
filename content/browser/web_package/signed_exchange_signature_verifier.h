// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_SIGNATURE_VERIFIER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_SIGNATURE_VERIFIER_H_

#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/common/content_export.h"
#include "net/cert/x509_certificate.h"

namespace base {
class Time;
}  // namespace base

namespace content {

class SignedExchangeCertificateChain;
class SignedExchangeEnvelope;
class SignedExchangeDevToolsProxy;

// SignedExchangeSignatureVerifier verifies the signature of the given
// signed exchange. This is done by reconstructing the signed message
// and verifying the cryptographic signature enclosed in "Signature" response
// header (given as |input.signature|).
//
// Note that SignedExchangeSignatureVerifier does not ensure the validity
// of the certificate used to generate the signature, which can't be done
// synchronously. (See SignedExchangeCertFetcher for this logic.)
//
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#signature-validity
class CONTENT_EXPORT SignedExchangeSignatureVerifier final {
 public:
  // This enum is used for recording histograms. Treat as append-only.
  enum class Result {
    kSuccess,
    kErrNoCertificate_deprecated,
    kErrNoCertificateSHA256_deprecated,
    kErrCertificateSHA256Mismatch,
    kErrInvalidSignatureFormat_deprecated,
    kErrSignatureVerificationFailed,
    kErrInvalidSignatureIntegrity_deprecated,
    kErrInvalidTimestamp_deprecated,
    kErrUnsupportedCertType,
    kErrValidityPeriodTooLong,
    kErrFutureDate,
    kErrExpired,
    kMaxValue = kErrExpired
  };

  static Result Verify(SignedExchangeVersion version,
                       const SignedExchangeEnvelope& envelope,
                       const SignedExchangeCertificateChain* cert_chain,
                       const base::Time& verification_time,
                       SignedExchangeDevToolsProxy* devtools_proxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_SIGNATURE_VERIFIER_H_

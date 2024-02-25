// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ERROR_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ERROR_H_

#include <optional>
#include <string>
#include <utility>

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

namespace content {

// This enum is used for recording histograms. Treat as append-only.
enum class SignedExchangeLoadResult {
  kSuccess,
  // SXG was served from non-secure origin.
  kSXGServedFromNonHTTPS,
  // SXG parse error (couldn't extract fallback URL).
  kFallbackURLParseError,
  // Unsupported version of SXG (could extract fallback URL).
  kVersionMismatch,
  // SXG parse error (could extract fallback URL).
  kHeaderParseError,
  // Network error occurred while loading SXG header.
  kSXGHeaderNetError,
  // Failed to fetch certificate chain.
  kCertFetchError,
  // Failed to parse certificate chain.
  kCertParseError,
  // Signature verification failed.
  kSignatureVerificationError,
  // Cert verification failed.
  kCertVerificationError,
  // CT verification failed.
  kCTVerificationError,
  // OCSP check failed.
  kOCSPError,
  // Certificate Requirements aren't met.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-cert-req
  kCertRequirementsNotMet,
  // SXG was served without "X-Content-Type-Options: nosniff" header.
  kSXGServedWithoutNosniff,
  // Merkle integrity error.
  kMerkleIntegrityError,
  // Invalid integrity header error.
  kInvalidIntegrityHeader,
  // SXG has Variants / Variant-Key headers that don't match the request.
  kVariantMismatch,
  // Certificate's validity period is too long.
  kCertValidityPeriodTooLong,
  // SXG had "Vary: Cookie" inner header but we had a cookie for the URL.
  kHadCookieForCookielessOnlySXG,
  // The certificate didn't match the built-in public key pins for the host
  // name.
  kPKPViolationError,
  kMaxValue = kPKPViolationError
};

struct SignedExchangeError {
 public:
  enum class Field {
    kSignatureSig,
    kSignatureIintegrity,
    kSignatureCertUrl,
    kSignatureCertSha256,
    kSignatureValidityUrl,
    kSignatureTimestamps,
  };

  // |signature_index| will be used when we will support multiple signatures in
  // a signed exchange header to indicate which signature is causing the error.
  using FieldIndexPair = std::pair<int /* signature_index */, Field>;

  static std::optional<Field> GetFieldFromSignatureVerifierResult(
      SignedExchangeSignatureVerifier::Result verify_result);

  SignedExchangeError(const std::string& message,
                      std::optional<FieldIndexPair> field);

  // Copy constructor.
  SignedExchangeError(const SignedExchangeError& other);
  // Move constructor.
  SignedExchangeError(SignedExchangeError&& other);

  ~SignedExchangeError();

  std::string message;
  std::optional<FieldIndexPair> field;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ERROR_H_

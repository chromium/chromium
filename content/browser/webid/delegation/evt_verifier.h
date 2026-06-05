// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EVT_VERIFIER_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EVT_VERIFIER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content::webid {

// This class encapsulates the verification steps for the Email Verification
// Token (EVT) and Key Binding JWT (KB-JWT) as described in the EVP spec.
class CONTENT_EXPORT EvtVerifier {
 public:
  EvtVerifier() = delete;

  // Verifies the serialized token (EVT+KB).
  // `token` is the serialized combined token string.
  // `issuer` is the expected issuer origin.
  // `issuer_pub_keys` is the parsed JWKS response.
  // `audience` is the expected audience (RP origin).
  // `email` is the expected email address.
  // `nonce` is the expected session nonce.
  // `holder_pub_key` is the browser's public key that was bound in the EVT.
  enum class Result {
    kVerified,
    kInvalidSdJwtKb,
    kSdJwtUnsupportedHeaderAlg,
    kSdJwtMissingIss,
    kSdJwtMissingIat,
    kSdJwtMissingCnf,
    kSdJwtMissingEmail,
    kSdJwtInvalidIssuedAt,
    kSdJwtInvalidIssuer,
    kSdJwtJwksMissingKeys,
    kSdJwtSignatureFailed,
    kSdJwtInvalidEmailVerified,
    kSdJwtInvalidEmail,
    kSdJwtInvalidHolderKey,
    kKbInvalidTyp,
    kKbMissingAud,
    kKbMissingNonce,
    kKbMissingIat,
    kKbMissingSdHash,
    kKbInvalidIssuedAt,
    kKbInvalidAudience,
    kKbInvalidNonce,
    kKbInvalidSdHash,
    kKbMissingCnf,
    kKbSignatureFailed,
  };

  static Result Verify(const std::string& token,
                       const url::Origin& issuer,
                       const base::DictValue& issuer_pub_keys,
                       const url::Origin& audience,
                       const std::string& email,
                       const std::string& nonce,
                       const sdjwt::Jwk& holder_pub_key);
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EVT_VERIFIER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/evt_verifier.h"

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "crypto/sha2.h"
#include "crypto/sign.h"

namespace content::webid {

using Result = EvtVerifier::Result;

namespace {

base::expected<void, Result> VerifyEVT(const sdjwt::SdJwt& sd_jwt,
                                       const sdjwt::Header& header,
                                       const sdjwt::Payload& payload,
                                       const base::DictValue& issuer_pub_keys,
                                       const url::Origin& issuer,
                                       const std::string& email,
                                       const sdjwt::Jwk& holder_pub_key) {
  if (header.alg != "EdDSA" && header.alg != "RS256" && header.alg != "ES256") {
    return base::unexpected(Result::kSdJwtUnsupportedHeaderAlg);
  }

  if (payload.iss.empty()) {
    return base::unexpected(Result::kSdJwtMissingIss);
  }
  if (!payload.iat) {
    return base::unexpected(Result::kSdJwtMissingIat);
  }
  if (!payload.cnf) {
    return base::unexpected(Result::kSdJwtMissingCnf);
  }
  if (payload.email.empty()) {
    return base::unexpected(Result::kSdJwtMissingEmail);
  }

  base::Time now = base::Time::Now();
  base::Time issued_at = *payload.iat;
  if (issued_at > now + base::Minutes(1) ||
      issued_at < now - base::Minutes(5)) {
    return base::unexpected(Result::kSdJwtInvalidIssuedAt);
  }

  if (payload.iss != issuer.Serialize()) {
    return base::unexpected(Result::kSdJwtInvalidIssuer);
  }

  const base::ListValue* keys = issuer_pub_keys.FindList("keys");
  if (!keys) {
    return base::unexpected(Result::kSdJwtJwksMissingKeys);
  }

  bool verified = false;
  for (const auto& key_value : *keys) {
    const base::DictValue* key_dict = key_value.GetIfDict();
    if (!key_dict) {
      continue;
    }

    const std::string* key_id = key_dict->FindString("kid");

    if (!header.kid.empty()) {
      if (!key_id || *key_id != header.kid) {
        continue;
      }
    }

    auto jwk_parsed = sdjwt::Jwk::From(*key_dict);
    if (jwk_parsed) {
      auto verifier = sdjwt::CreateJwtVerifier(*jwk_parsed, header);
      if (verifier && sd_jwt.jwt.Verify(std::move(verifier))) {
        verified = true;
        break;
      }
    }
  }

  if (!verified) {
    return base::unexpected(Result::kSdJwtSignatureFailed);
  }

  if (!payload.email_verified) {
    return base::unexpected(Result::kSdJwtInvalidEmailVerified);
  }

  if (!base::EqualsCaseInsensitiveASCII(payload.email, email)) {
    return base::unexpected(Result::kSdJwtInvalidEmail);
  }

  if (holder_pub_key != payload.cnf->jwk) {
    return base::unexpected(Result::kSdJwtInvalidHolderKey);
  }

  return base::ok();
}

base::expected<void, Result> VerifyKB(const sdjwt::Jwt& kb_jwt,
                                      const sdjwt::Header& kb_header,
                                      const sdjwt::Payload& kb_payload,
                                      const sdjwt::Payload& evt_payload,
                                      const std::string& evt_string,
                                      const url::Origin& audience,
                                      const std::string& nonce) {
  if (kb_header.typ != "kb+jwt") {
    return base::unexpected(Result::kKbInvalidTyp);
  }

  if (kb_payload.aud.empty()) {
    return base::unexpected(Result::kKbMissingAud);
  }
  if (kb_payload.nonce.empty()) {
    return base::unexpected(Result::kKbMissingNonce);
  }
  if (!kb_payload.iat) {
    return base::unexpected(Result::kKbMissingIat);
  }
  if (kb_payload.sd_hash.value().empty()) {
    return base::unexpected(Result::kKbMissingSdHash);
  }

  base::Time now = base::Time::Now();
  base::Time issued_at = *kb_payload.iat;
  if (issued_at > now + base::Minutes(1) ||
      issued_at < now - base::Minutes(5)) {
    return base::unexpected(Result::kKbInvalidIssuedAt);
  }

  // - Verify aud matches the RP's origin
  if (kb_payload.aud != audience.Serialize()) {
    return base::unexpected(Result::kKbInvalidAudience);
  }

  // - Verify nonce matches the nonce from the RP's session
  if (kb_payload.nonce != nonce) {
    return base::unexpected(Result::kKbInvalidNonce);
  }

  // - Compute the SHA-256 hash of the EVT and verify it matches sd_hash
  std::string computed_hash = crypto::SHA256HashString(evt_string);
  std::string computed_hash_b64;
  base::Base64UrlEncode(computed_hash,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &computed_hash_b64);

  if (kb_payload.sd_hash.value() != computed_hash_b64) {
    return base::unexpected(Result::kKbInvalidSdHash);
  }

  if (!evt_payload.cnf) {
    return base::unexpected(Result::kKbMissingCnf);
  }
  auto holder_pub_key = evt_payload.cnf->jwk;

  // - Verify the KB-JWT signature using the public key from the EVT's cnf.jwk
  // claim
  auto verifier = sdjwt::CreateJwtVerifier(holder_pub_key, kb_header);
  if (!verifier || !kb_jwt.Verify(std::move(verifier))) {
    return base::unexpected(Result::kKbSignatureFailed);
  }

  return base::ok();
}
}  // namespace

EvtVerifier::Result EvtVerifier::Verify(const std::string& token,
                                        const url::Origin& issuer,
                                        const base::DictValue& issuer_pub_keys,
                                        const url::Origin& audience,
                                        const std::string& email,
                                        const std::string& nonce,
                                        const sdjwt::Jwk& holder_pub_key) {
  auto sd_jwt_kb = sdjwt::SdJwtKb::Parse(token);
  if (!sd_jwt_kb) {
    return Result::kInvalidSdJwtKb;
  }

  std::optional<base::DictValue> header_dict = base::JSONReader::ReadDict(
      sd_jwt_kb->sd_jwt.jwt.header.value(), base::JSON_PARSE_RFC);
  std::optional<base::DictValue> payload_dict = base::JSONReader::ReadDict(
      sd_jwt_kb->sd_jwt.jwt.payload.value(), base::JSON_PARSE_RFC);
  std::optional<base::DictValue> kb_header_dict = base::JSONReader::ReadDict(
      sd_jwt_kb->kb_jwt.header.value(), base::JSON_PARSE_RFC);
  std::optional<base::DictValue> kb_payload_dict = base::JSONReader::ReadDict(
      sd_jwt_kb->kb_jwt.payload.value(), base::JSON_PARSE_RFC);

  if (!header_dict || !payload_dict || !kb_header_dict || !kb_payload_dict) {
    return Result::kInvalidSdJwtKb;
  }

  auto header = sdjwt::Header::From(*header_dict);
  auto payload = sdjwt::Payload::From(*payload_dict);
  auto kb_header = sdjwt::Header::From(*kb_header_dict);
  auto kb_payload = sdjwt::Payload::From(*kb_payload_dict);

  if (!header || !payload || !kb_header || !kb_payload) {
    return Result::kInvalidSdJwtKb;
  }

  // 1. Verify EVT part.
  if (auto result = VerifyEVT(sd_jwt_kb->sd_jwt, *header, *payload,
                              issuer_pub_keys, issuer, email, holder_pub_key);
      result != base::ok()) {
    return result.error();
  }

  // 2. Verify KB part.
  if (auto result =
          VerifyKB(sd_jwt_kb->kb_jwt, *kb_header, *kb_payload, *payload,
                   sd_jwt_kb->sd_jwt.Serialize(), audience, nonce);
      result != base::ok()) {
    return result.error();
  }

  return Result::kVerified;
}

}  // namespace content::webid

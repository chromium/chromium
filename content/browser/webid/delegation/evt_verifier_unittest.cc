// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/evt_verifier.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

namespace {

struct TokenContext {
  std::string full_token;
  url::Origin issuer_origin;
  base::DictValue jwks;
  url::Origin rp_origin;
  std::string email;
  std::string nonce;
  sdjwt::Jwk browser_jwk;
};

struct TokenOptions {
  std::string evt_typ = "evt+jwt";
  std::string evt_alg = "EdDSA";
  std::optional<std::string> evt_kid = "valid_kid";
  std::string evt_iss = "https://issuer.example.com";
  std::string evt_email = "test@example.com";
  base::Time evt_iat = base::Time::Now();

  std::string kb_typ = "kb+jwt";
  std::string kb_alg = "EdDSA";
  std::string kb_aud = "https://rp.example.com";
  std::string kb_nonce = "test_nonce";
  base::Time kb_iat = base::Time::Now();

  std::string expected_issuer = "https://issuer.example.com";
  std::string expected_email = "test@example.com";
  std::string expected_nonce = "test_nonce";
  std::string expected_rp = "https://rp.example.com";
};

TokenContext CreateTokenContext(const TokenOptions& options = TokenOptions()) {
  // 1. Generate Keys
  auto issuer_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto browser_key = crypto::keypair::PrivateKey::GenerateEd25519();

  // 2. Construct JWKS for Issuer
  base::DictValue jwks;
  base::ListValue keys;
  base::DictValue key_dict = sdjwt::ExportPublicKey(issuer_key)->ToDict();
  key_dict.Set("kid", "valid_kid");
  keys.Append(std::move(key_dict));
  jwks.Set("keys", std::move(keys));

  // 3. Construct Browser JWK for cnf claim
  sdjwt::Jwk browser_jwk = *sdjwt::ExportPublicKey(browser_key);

  // 4. Construct and Sign EVT
  sdjwt::SdJwt token;
  sdjwt::Header h;
  h.typ = options.evt_typ;
  h.alg = options.evt_alg;
  if (options.evt_kid) {
    h.kid = *options.evt_kid;
  }

  sdjwt::Payload p;
  p.iss = options.evt_iss;
  p.email = options.evt_email;
  p.email_verified = true;
  p.iat = options.evt_iat;
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *h.ToJson();
  issued_jwt.payload = *p.ToJson();
  CHECK(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  // 5. Construct and Sign KB-JWT
  sdjwt::Header kb_header;
  kb_header.alg = options.kb_alg;
  kb_header.typ = options.kb_typ;

  sdjwt::Payload kb_payload;
  kb_payload.aud = options.kb_aud;
  kb_payload.nonce = options.kb_nonce;
  kb_payload.iat = options.kb_iat;

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *kb_header.ToJson();
  kb_jwt.payload = *kb_payload.ToJson();

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  CHECK(kb_jwt.Sign(std::move(browser_signer)));

  return TokenContext{
      .full_token = evt_string + kb_jwt.Serialize().value(),
      .issuer_origin = url::Origin::Create(GURL(options.expected_issuer)),
      .jwks = std::move(jwks),
      .rp_origin = url::Origin::Create(GURL(options.expected_rp)),
      .email = options.expected_email,
      .nonce = options.expected_nonce,
      .browser_jwk = browser_jwk,
  };
}

void PrependKey(base::ListValue* keys,
                std::optional<std::string> kid = std::nullopt) {
  CHECK(keys);
  auto key = crypto::keypair::PrivateKey::GenerateEd25519();
  base::DictValue key_dict = sdjwt::ExportPublicKey(key)->ToDict();
  if (kid) {
    key_dict.Set("kid", *kid);
  }
  keys->Insert(keys->begin(), base::Value(std::move(key_dict)));
}

}  // namespace

class EvtVerifierTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(EvtVerifierTest, SuccessfulVerification) {
  auto ctx = CreateTokenContext();
  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, CaseInsensitiveEmailMatch) {
  TokenOptions options;
  options.evt_email = "TeSt@ExAmPlE.CoM";
  options.expected_email = "test@example.com";
  auto ctx = CreateTokenContext(options);
  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, ExpiredEvtRejected) {
  TokenOptions options;
  options.evt_iat = base::Time::Now() - base::Minutes(6);
  auto ctx = CreateTokenContext(options);
  EXPECT_NE(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, ExpiredKbRejected) {
  TokenOptions options;
  options.kb_iat = base::Time::Now() - base::Minutes(6);
  auto ctx = CreateTokenContext(options);
  EXPECT_NE(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, MismatchedIssuerRejected) {
  TokenOptions options;
  options.expected_issuer = "https://mismatched.example.com";
  auto ctx = CreateTokenContext(options);
  EXPECT_NE(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, VerificationFallbackWhenKidMissing) {
  TokenOptions options;
  options.evt_kid = std::nullopt;
  auto ctx = CreateTokenContext(options);

  // Prepend an unmatching key to JWKS to verify fallback iterates through keys.
  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys);
  PrependKey(keys, "unmatching_kid");

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, VerificationFallbackWhenKidEmpty) {
  TokenOptions options;
  options.evt_kid = "";
  auto ctx = CreateTokenContext(options);

  // Prepend an unmatching key to JWKS to verify fallback iterates through keys.
  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys);
  PrependKey(keys, "unmatching_kid");

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, VerificationFallbackWhenBothHeaderAndJwksHaveNoKid) {
  TokenOptions options;
  options.evt_kid = std::nullopt;
  auto ctx = CreateTokenContext(options);

  // Remove "kid" from the valid JWKS key so that the JWKS key has no kid.
  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys && !keys->empty());
  base::DictValue* valid_key_dict = (*keys)[0].GetIfDict();
  ASSERT_TRUE(valid_key_dict);
  valid_key_dict->Remove("kid");

  // Also prepend an unmatching key (without kid) to verify fallback iterates
  // through keys and successfully finds the valid key.
  PrependKey(keys);

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, MismatchedKidInTokenRejected) {
  TokenOptions options;
  options.evt_kid = "nonexistent_kid";
  auto ctx = CreateTokenContext(options);
  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kSdJwtSignatureFailed);
}

TEST_F(EvtVerifierTest, VerificationWithMatchingKidInMultiKeyJwks) {
  TokenOptions options;
  options.evt_kid = "valid_kid";
  auto ctx = CreateTokenContext(options);

  // Prepend a key with a different kid. Verifier should skip the prepended key
  // based on kid and find the matching valid_kid.
  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys);
  PrependKey(keys, "other_kid");

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, NoFallbackWhenKidSpecified) {
  TokenOptions options;
  options.evt_kid = "target_kid";
  auto ctx = CreateTokenContext(options);

  // Prepend a key whose kid matches the header's target_kid ("target_kid").
  // Since kid is specified, it must only test the key matching "target_kid",
  // which does not match the token's signature, and must NOT fall back to
  // "valid_kid".
  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys);
  PrependKey(keys, "target_kid");

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kSdJwtSignatureFailed);
}

TEST_F(EvtVerifierTest, TokenSpecifiesKidJwksOmitsKidRejected) {
  TokenOptions options;
  options.evt_kid = "valid_kid";
  auto ctx = CreateTokenContext(options);

  // Remove "kid" from the valid JWKS key so that the JWKS key has no kid.
  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys && !keys->empty());
  base::DictValue* valid_key_dict = (*keys)[0].GetIfDict();
  ASSERT_TRUE(valid_key_dict);
  valid_key_dict->Remove("kid");

  // Also prepend an unmatching key without kid.
  PrependKey(keys);

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kSdJwtSignatureFailed);
}

TEST_F(EvtVerifierTest, InvalidTypRejected) {
  TokenOptions options;
  options.evt_typ = "invalid+typ";
  auto ctx = CreateTokenContext(options);
  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kSdJwtInvalidTyp);
}

TEST_F(EvtVerifierTest, InvalidIssuerInTokenRejected) {
  const std::vector<std::string> kInvalidIssuers = {
      "issuer.example.com",                // missing scheme
      "http://issuer.example.com",         // non-https scheme
      "https://issuer.example.com/path",   // contains path
      "https://issuer.example.com?query",  // contains query
      "https://issuer.example.com:90",     // port mismatch
      "https://issuer.example.com:443",    // explicit default port
                                           // (non-canonical)
      "https://other.example.com",         // mismatched issuer
      "invalid_url",                       // malformed
  };

  for (const auto& invalid_iss : kInvalidIssuers) {
    SCOPED_TRACE(invalid_iss);

    TokenOptions options;
    options.evt_iss = invalid_iss;
    auto ctx = CreateTokenContext(options);

    EXPECT_EQ(EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                                  ctx.rp_origin, ctx.email, ctx.nonce,
                                  ctx.browser_jwk),
              EvtVerifier::Result::kSdJwtInvalidIssuer);
  }
}

}  // namespace content::webid

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
  std::optional<std::string> evt_kid = "test_kid";
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
  auto issuer_pub_bytes = crypto::keypair::PublicKey::FromPrivateKey(issuer_key)
                              .ToEd25519PublicKey();

  auto browser_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto browser_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(browser_key)
          .ToEd25519PublicKey();

  // 2. Construct JWKS for Issuer
  base::DictValue jwks;
  base::ListValue keys;
  base::DictValue key_dict;
  key_dict.Set("kty", "OKP");
  key_dict.Set("crv", "Ed25519");
  key_dict.Set("kid", options.evt_kid.value_or("valid_kid"));
  std::string x_b64;
  base::Base64UrlEncode(issuer_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &x_b64);
  key_dict.Set("x", x_b64);
  keys.Append(std::move(key_dict));
  jwks.Set("keys", std::move(keys));

  // 3. Construct Browser JWK for cnf claim
  sdjwt::Jwk browser_jwk;
  browser_jwk.kty = "OKP";
  browser_jwk.crv = "Ed25519";
  base::Base64UrlEncode(browser_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &browser_jwk.x);

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

  // Prepend an invalid key to JWKS to verify fallback iterates through keys.
  auto invalid_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto invalid_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(invalid_key)
          .ToEd25519PublicKey();
  base::DictValue invalid_key_dict;
  invalid_key_dict.Set("kty", "OKP");
  invalid_key_dict.Set("crv", "Ed25519");
  invalid_key_dict.Set("kid", "invalid_kid");
  std::string inv_x_b64;
  base::Base64UrlEncode(invalid_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &inv_x_b64);
  invalid_key_dict.Set("x", inv_x_b64);

  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys);
  keys->Insert(keys->begin(), base::Value(std::move(invalid_key_dict)));

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, VerificationFallbackWhenKidEmpty) {
  TokenOptions options;
  options.evt_kid = "";
  auto ctx = CreateTokenContext(options);

  // Prepend an invalid key to JWKS to verify fallback iterates through keys.
  auto invalid_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto invalid_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(invalid_key)
          .ToEd25519PublicKey();
  base::DictValue invalid_key_dict;
  invalid_key_dict.Set("kty", "OKP");
  invalid_key_dict.Set("crv", "Ed25519");
  invalid_key_dict.Set("kid", "invalid_kid");
  std::string inv_x_b64;
  base::Base64UrlEncode(invalid_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &inv_x_b64);
  invalid_key_dict.Set("x", inv_x_b64);

  base::ListValue* keys = ctx.jwks.FindList("keys");
  ASSERT_TRUE(keys);
  keys->Insert(keys->begin(), base::Value(std::move(invalid_key_dict)));

  EXPECT_EQ(
      EvtVerifier::Verify(ctx.full_token, ctx.issuer_origin, ctx.jwks,
                          ctx.rp_origin, ctx.email, ctx.nonce, ctx.browser_jwk),
      EvtVerifier::Result::kVerified);
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

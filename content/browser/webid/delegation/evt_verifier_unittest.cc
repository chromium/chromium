// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/evt_verifier.h"

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::webid {

class EvtVerifierTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(EvtVerifierTest, SuccessfulVerification) {
  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const std::string kRpOrigin = "https://rp.example.com";
  const std::string kIssuerUrl = "https://issuer.example.com";

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
  key_dict.Set("kid", "test_kid");
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
  h.typ = "evp+sd-jwt";
  h.alg = "EdDSA";
  h.kid = "test_kid";

  sdjwt::Payload p;
  p.iss = kIssuerUrl;
  p.email = kEmail;
  p.email_verified = true;
  p.iat = base::Time::Now();
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *(h.ToJson());
  issued_jwt.payload = *(p.ToJson());
  ASSERT_TRUE(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  // 5. Construct and Sign KB-JWT
  sdjwt::Header kb_header;
  kb_header.alg = "EdDSA";
  kb_header.typ = "kb+jwt";

  sdjwt::Payload kb_payload;
  kb_payload.aud = kRpOrigin;
  kb_payload.nonce = kNonce;
  kb_payload.iat = base::Time::Now();

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *(kb_header.ToJson());
  kb_jwt.payload = *(kb_payload.ToJson());

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  ASSERT_TRUE(kb_jwt.Sign(std::move(browser_signer)));

  // 6. Combine Tokens
  std::string full_token = evt_string + kb_jwt.Serialize().value();

  // 7. Verify
  base::test::TestFuture<EvtVerifier::Result> future;
  EvtVerifier::Verify(full_token, url::Origin::Create(GURL(kIssuerUrl)),
                      std::move(jwks), url::Origin::Create(GURL(kRpOrigin)),
                      kEmail, kNonce, browser_jwk, future.GetCallback());

  EXPECT_EQ(future.Get(), EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, CaseInsensitiveEmailMatch) {
  const std::string kEmail = "test@example.com";
  const std::string kEmailMixedCase = "TeSt@ExAmPlE.CoM";
  const std::string kNonce = "test_nonce";
  const std::string kRpOrigin = "https://rp.example.com";
  const std::string kIssuerUrl = "https://issuer.example.com";

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
  key_dict.Set("kid", "test_kid");
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
  h.typ = "evp+sd-jwt";
  h.alg = "EdDSA";
  h.kid = "test_kid";

  sdjwt::Payload p;
  p.iss = kIssuerUrl;
  p.email = kEmail;
  p.email_verified = true;
  p.iat = base::Time::Now();
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *h.ToJson();
  issued_jwt.payload = *p.ToJson();
  ASSERT_TRUE(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  // 5. Construct and Sign KB-JWT
  sdjwt::Header kb_header;
  kb_header.alg = "EdDSA";
  kb_header.typ = "kb+jwt";

  sdjwt::Payload kb_payload;
  kb_payload.aud = kRpOrigin;
  kb_payload.nonce = kNonce;
  kb_payload.iat = base::Time::Now();

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *kb_header.ToJson();
  kb_jwt.payload = *kb_payload.ToJson();

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  ASSERT_TRUE(kb_jwt.Sign(std::move(browser_signer)));

  // 6. Combine Tokens
  std::string full_token = evt_string + kb_jwt.Serialize().value();

  // 7. Verify
  base::test::TestFuture<EvtVerifier::Result> future;
  EvtVerifier::Verify(full_token, url::Origin::Create(GURL(kIssuerUrl)),
                      std::move(jwks), url::Origin::Create(GURL(kRpOrigin)),
                      kEmailMixedCase, kNonce, browser_jwk,
                      future.GetCallback());

  EXPECT_EQ(future.Get(), EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, ExpiredEvtRejected) {
  auto issuer_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto issuer_pub_bytes = crypto::keypair::PublicKey::FromPrivateKey(issuer_key)
                              .ToEd25519PublicKey();

  auto browser_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto browser_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(browser_key)
          .ToEd25519PublicKey();

  base::DictValue jwks;
  base::ListValue keys;
  base::DictValue key_dict;
  key_dict.Set("kty", "OKP");
  key_dict.Set("crv", "Ed25519");
  key_dict.Set("kid", "test_kid");
  std::string x_b64;
  base::Base64UrlEncode(issuer_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &x_b64);
  key_dict.Set("x", x_b64);
  keys.Append(std::move(key_dict));
  jwks.Set("keys", std::move(keys));

  sdjwt::Jwk browser_jwk;
  browser_jwk.kty = "OKP";
  browser_jwk.crv = "Ed25519";
  base::Base64UrlEncode(browser_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &browser_jwk.x);

  sdjwt::SdJwt token;
  sdjwt::Header h;
  h.typ = "evp+sd-jwt";
  h.alg = "EdDSA";
  h.kid = "test_kid";

  sdjwt::Payload p;
  p.iss = "https://issuer.example.com";
  p.email = "test@example.com";
  p.email_verified = true;
  p.iat = base::Time::Now() - base::Minutes(6);
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *(h.ToJson());
  issued_jwt.payload = *(p.ToJson());
  ASSERT_TRUE(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  sdjwt::Header kb_header;
  kb_header.alg = "EdDSA";
  kb_header.typ = "kb+jwt";

  sdjwt::Payload kb_payload;
  kb_payload.aud = "https://rp.example.com";
  kb_payload.nonce = "test_nonce";
  kb_payload.iat = base::Time::Now();

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *(kb_header.ToJson());
  kb_jwt.payload = *(kb_payload.ToJson());

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  ASSERT_TRUE(kb_jwt.Sign(std::move(browser_signer)));

  std::string full_token = evt_string + kb_jwt.Serialize().value();

  base::test::TestFuture<EvtVerifier::Result> future;
  EvtVerifier::Verify(
      full_token, url::Origin::Create(GURL("https://issuer.example.com")),
      std::move(jwks), url::Origin::Create(GURL("https://rp.example.com")),
      "test@example.com", "test_nonce", browser_jwk, future.GetCallback());

  EXPECT_NE(future.Get(), EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, ExpiredKbRejected) {
  auto issuer_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto issuer_pub_bytes = crypto::keypair::PublicKey::FromPrivateKey(issuer_key)
                              .ToEd25519PublicKey();

  auto browser_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto browser_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(browser_key)
          .ToEd25519PublicKey();

  base::DictValue jwks;
  base::ListValue keys;
  base::DictValue key_dict;
  key_dict.Set("kty", "OKP");
  key_dict.Set("crv", "Ed25519");
  key_dict.Set("kid", "test_kid");
  std::string x_b64;
  base::Base64UrlEncode(issuer_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &x_b64);
  key_dict.Set("x", x_b64);
  keys.Append(std::move(key_dict));
  jwks.Set("keys", std::move(keys));

  sdjwt::Jwk browser_jwk;
  browser_jwk.kty = "OKP";
  browser_jwk.crv = "Ed25519";
  base::Base64UrlEncode(browser_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &browser_jwk.x);

  sdjwt::SdJwt token;
  sdjwt::Header h;
  h.typ = "evp+sd-jwt";
  h.alg = "EdDSA";
  h.kid = "test_kid";

  sdjwt::Payload p;
  p.iss = "https://issuer.example.com";
  p.email = "test@example.com";
  p.email_verified = true;
  p.iat = base::Time::Now();
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *(h.ToJson());
  issued_jwt.payload = *(p.ToJson());
  ASSERT_TRUE(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  sdjwt::Header kb_header;
  kb_header.alg = "EdDSA";
  kb_header.typ = "kb+jwt";

  sdjwt::Payload kb_payload;
  kb_payload.aud = "https://rp.example.com";
  kb_payload.nonce = "test_nonce";
  kb_payload.iat = base::Time::Now() - base::Minutes(6);

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *(kb_header.ToJson());
  kb_jwt.payload = *(kb_payload.ToJson());

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  ASSERT_TRUE(kb_jwt.Sign(std::move(browser_signer)));

  std::string full_token = evt_string + kb_jwt.Serialize().value();

  base::test::TestFuture<EvtVerifier::Result> future;
  EvtVerifier::Verify(
      full_token, url::Origin::Create(GURL("https://issuer.example.com")),
      std::move(jwks), url::Origin::Create(GURL("https://rp.example.com")),
      "test@example.com", "test_nonce", browser_jwk, future.GetCallback());

  EXPECT_NE(future.Get(), EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, MismatchedIssuerRejected) {
  auto issuer_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto issuer_pub_bytes = crypto::keypair::PublicKey::FromPrivateKey(issuer_key)
                              .ToEd25519PublicKey();

  auto browser_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto browser_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(browser_key)
          .ToEd25519PublicKey();

  base::DictValue jwks;
  base::ListValue keys;
  base::DictValue key_dict;
  key_dict.Set("kty", "OKP");
  key_dict.Set("crv", "Ed25519");
  key_dict.Set("kid", "test_kid");
  std::string x_b64;
  base::Base64UrlEncode(issuer_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &x_b64);
  key_dict.Set("x", x_b64);
  keys.Append(std::move(key_dict));
  jwks.Set("keys", std::move(keys));

  sdjwt::Jwk browser_jwk;
  browser_jwk.kty = "OKP";
  browser_jwk.crv = "Ed25519";
  base::Base64UrlEncode(browser_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &browser_jwk.x);

  sdjwt::SdJwt token;
  sdjwt::Header h;
  h.typ = "evp+sd-jwt";
  h.alg = "EdDSA";
  h.kid = "test_kid";

  sdjwt::Payload p;
  p.iss = "https://issuer.example.com";
  p.email = "test@example.com";
  p.email_verified = true;
  p.iat = base::Time::Now();
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *(h.ToJson());
  issued_jwt.payload = *(p.ToJson());
  ASSERT_TRUE(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  sdjwt::Header kb_header;
  kb_header.alg = "EdDSA";
  kb_header.typ = "kb+jwt";

  sdjwt::Payload kb_payload;
  kb_payload.aud = "https://rp.example.com";
  kb_payload.nonce = "test_nonce";
  kb_payload.iat = base::Time::Now();

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *(kb_header.ToJson());
  kb_jwt.payload = *(kb_payload.ToJson());

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  ASSERT_TRUE(kb_jwt.Sign(std::move(browser_signer)));

  std::string full_token = evt_string + kb_jwt.Serialize().value();

  base::test::TestFuture<EvtVerifier::Result> future;
  EvtVerifier::Verify(
      full_token, url::Origin::Create(GURL("https://mismatched.example.com")),
      std::move(jwks), url::Origin::Create(GURL("https://rp.example.com")),
      "test@example.com", "test_nonce", browser_jwk, future.GetCallback());

  EXPECT_NE(future.Get(), EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, VerificationFallbackWhenKidMissing) {
  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const std::string kRpOrigin = "https://rp.example.com";
  const std::string kIssuerUrl = "https://issuer.example.com";

  // 1. Generate Keys
  auto issuer_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto issuer_pub_bytes = crypto::keypair::PublicKey::FromPrivateKey(issuer_key)
                              .ToEd25519PublicKey();

  auto browser_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto browser_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(browser_key)
          .ToEd25519PublicKey();

  // Generate another key that won't match.
  auto invalid_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto invalid_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(invalid_key)
          .ToEd25519PublicKey();

  // 2. Construct JWKS for Issuer (with both keys)
  base::DictValue jwks;
  base::ListValue keys;

  // Invalid key first.
  base::DictValue invalid_key_dict;
  invalid_key_dict.Set("kty", "OKP");
  invalid_key_dict.Set("crv", "Ed25519");
  invalid_key_dict.Set("kid", "invalid_kid");
  std::string inv_x_b64;
  base::Base64UrlEncode(invalid_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &inv_x_b64);
  invalid_key_dict.Set("x", inv_x_b64);
  keys.Append(std::move(invalid_key_dict));

  // Valid key second.
  base::DictValue valid_key_dict;
  valid_key_dict.Set("kty", "OKP");
  valid_key_dict.Set("crv", "Ed25519");
  valid_key_dict.Set("kid", "valid_kid");
  std::string x_b64;
  base::Base64UrlEncode(issuer_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &x_b64);
  valid_key_dict.Set("x", x_b64);
  keys.Append(std::move(valid_key_dict));

  jwks.Set("keys", std::move(keys));

  // 3. Construct Browser JWK for cnf claim
  sdjwt::Jwk browser_jwk;
  browser_jwk.kty = "OKP";
  browser_jwk.crv = "Ed25519";
  base::Base64UrlEncode(browser_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &browser_jwk.x);

  // 4. Construct and Sign EVT (without kid)
  sdjwt::SdJwt token;
  sdjwt::Header h;
  h.typ = "evp+sd-jwt";
  h.alg = "EdDSA";
  // NOT setting h.kid!

  sdjwt::Payload p;
  p.iss = kIssuerUrl;
  p.email = kEmail;
  p.email_verified = true;
  p.iat = base::Time::Now();
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *(h.ToJson());
  issued_jwt.payload = *(p.ToJson());
  ASSERT_TRUE(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  // 5. Construct and Sign KB-JWT
  sdjwt::Header kb_header;
  kb_header.alg = "EdDSA";
  kb_header.typ = "kb+jwt";

  sdjwt::Payload kb_payload;
  kb_payload.aud = kRpOrigin;
  kb_payload.nonce = kNonce;
  kb_payload.iat = base::Time::Now();

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *(kb_header.ToJson());
  kb_jwt.payload = *(kb_payload.ToJson());

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  ASSERT_TRUE(kb_jwt.Sign(std::move(browser_signer)));

  // 6. Combine Tokens
  std::string full_token = evt_string + kb_jwt.Serialize().value();

  // 7. Verify
  base::test::TestFuture<EvtVerifier::Result> future;
  EvtVerifier::Verify(full_token, url::Origin::Create(GURL(kIssuerUrl)),
                      std::move(jwks), url::Origin::Create(GURL(kRpOrigin)),
                      kEmail, kNonce, browser_jwk, future.GetCallback());

  EXPECT_EQ(future.Get(), EvtVerifier::Result::kVerified);
}

TEST_F(EvtVerifierTest, VerificationFallbackWhenKidEmpty) {
  const std::string kEmail = "test@example.com";
  const std::string kNonce = "test_nonce";
  const std::string kRpOrigin = "https://rp.example.com";
  const std::string kIssuerUrl = "https://issuer.example.com";

  // 1. Generate Keys
  auto issuer_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto issuer_pub_bytes = crypto::keypair::PublicKey::FromPrivateKey(issuer_key)
                              .ToEd25519PublicKey();

  auto browser_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto browser_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(browser_key)
          .ToEd25519PublicKey();

  // Generate another key that won't match.
  auto invalid_key = crypto::keypair::PrivateKey::GenerateEd25519();
  auto invalid_pub_bytes =
      crypto::keypair::PublicKey::FromPrivateKey(invalid_key)
          .ToEd25519PublicKey();

  // 2. Construct JWKS for Issuer (with both keys)
  base::DictValue jwks;
  base::ListValue keys;

  // Invalid key first.
  base::DictValue invalid_key_dict;
  invalid_key_dict.Set("kty", "OKP");
  invalid_key_dict.Set("crv", "Ed25519");
  invalid_key_dict.Set("kid", "invalid_kid");
  std::string inv_x_b64;
  base::Base64UrlEncode(invalid_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &inv_x_b64);
  invalid_key_dict.Set("x", inv_x_b64);
  keys.Append(std::move(invalid_key_dict));

  // Valid key second.
  base::DictValue valid_key_dict;
  valid_key_dict.Set("kty", "OKP");
  valid_key_dict.Set("crv", "Ed25519");
  valid_key_dict.Set("kid", "valid_kid");
  std::string x_b64;
  base::Base64UrlEncode(issuer_pub_bytes,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &x_b64);
  valid_key_dict.Set("x", x_b64);
  keys.Append(std::move(valid_key_dict));

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
  h.typ = "evp+sd-jwt";
  h.alg = "EdDSA";
  h.kid = "";  // Set empty kid

  sdjwt::Payload p;
  p.iss = kIssuerUrl;
  p.email = kEmail;
  p.email_verified = true;
  p.iat = base::Time::Now();
  sdjwt::ConfirmationKey cnf;
  cnf.jwk = browser_jwk;
  p.cnf = cnf;

  auto issuer_signer = sdjwt::CreateJwtSigner(issuer_key);
  sdjwt::Jwt issued_jwt;
  issued_jwt.header = *(h.ToJson());
  issued_jwt.payload = *(p.ToJson());
  ASSERT_TRUE(issued_jwt.Sign(std::move(issuer_signer)));
  token.jwt = issued_jwt;

  std::string evt_string = token.Serialize();

  // 5. Construct and Sign KB-JWT
  sdjwt::Header kb_header;
  kb_header.alg = "EdDSA";
  kb_header.typ = "kb+jwt";

  sdjwt::Payload kb_payload;
  kb_payload.aud = kRpOrigin;
  kb_payload.nonce = kNonce;
  kb_payload.iat = base::Time::Now();

  std::string sd_jwt_sha256 = crypto::SHA256HashString(evt_string);
  std::string sd_hash;
  base::Base64UrlEncode(sd_jwt_sha256,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &sd_hash);
  kb_payload.sd_hash = sdjwt::Base64String(sd_hash);

  sdjwt::Jwt kb_jwt;
  kb_jwt.header = *(kb_header.ToJson());
  kb_jwt.payload = *(kb_payload.ToJson());

  auto browser_signer = sdjwt::CreateJwtSigner(browser_key);
  ASSERT_TRUE(kb_jwt.Sign(std::move(browser_signer)));

  // 6. Combine Tokens
  std::string full_token = evt_string + kb_jwt.Serialize().value();

  // 7. Verify
  base::test::TestFuture<EvtVerifier::Result> future;
  EvtVerifier::Verify(full_token, url::Origin::Create(GURL(kIssuerUrl)),
                      std::move(jwks), url::Origin::Create(GURL(kRpOrigin)),
                      kEmail, kNonce, browser_jwk, future.GetCallback());

  EXPECT_EQ(future.Get(), EvtVerifier::Result::kVerified);
}

}  // namespace content::webid

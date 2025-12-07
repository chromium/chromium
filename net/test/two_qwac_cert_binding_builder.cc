// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/two_qwac_cert_binding_builder.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/json/json_string_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {

TwoQwacCertBindingBuilder::TwoQwacCertBindingBuilder()
    : cert_chain_(CertBuilder::CreateSimpleChain(2)) {
  GetLeafBuilder()->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen
  GetLeafBuilder()->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});
  GetLeafBuilder()->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});
  GenerateKeyForSigAlg();
  // set bound_certs_ to two bogus values
  bound_certs_ = {"one", "two"};
}

TwoQwacCertBindingBuilder::~TwoQwacCertBindingBuilder() = default;

void TwoQwacCertBindingBuilder::GenerateKeyForSigAlg() {
  switch (sig_alg_) {
    case JwsSigAlg::kRsaPkcs1Sha256:
    case JwsSigAlg::kRsaPssSha256:
      cert_chain_[0]->GenerateRSAKey();
      break;
    case JwsSigAlg::kEcdsaP256Sha256:
      cert_chain_[0]->GenerateECKey();
  }
  Invalidate();
}

std::string TwoQwacCertBindingBuilder::SigAlg() const {
  switch (sig_alg_) {
    case JwsSigAlg::kRsaPkcs1Sha256:
      return "RS256";
    case JwsSigAlg::kRsaPssSha256:
      return "PS256";
    case JwsSigAlg::kEcdsaP256Sha256:
      return "ES256";
  }
  return "";
}

std::string TwoQwacCertBindingBuilder::HashAlg() const {
  switch (hash_alg_) {
    case crypto::hash::kSha256:
      return "S256";
    case crypto::hash::kSha384:
      return "S384";
    case crypto::hash::kSha512:
      return "S512";
    default:
      return "";
  }
}

base::ListValue TwoQwacCertBindingBuilder::GenerateX5cHeaderValue() {
  base::ListValue x5c_list;
  for (const auto& cert : cert_chain_) {
    x5c_list.Append(base::Base64Encode(cert->GetDER()));
  }
  return x5c_list;
}

base::DictValue TwoQwacCertBindingBuilder::GenerateSigDHeaderValue() {
  base::DictValue sig_d =
      base::Value::Dict()
          .Set("mId", "http://uri.etsi.org/19182/ObjectIdByURIHash")
          .Set("hashM", HashAlg());
  base::ListValue pars;
  base::ListValue hash_v;
  for (const auto& bound_cert : bound_certs_) {
    // ETSI TS 119 182-1 clause 5.2.8.1: Each element of the "hashV" array
    // shall contain the base64url-encoded digest value of the
    // base64url-encoded data object.
    std::string cert_b64;
    base::Base64UrlEncode(bound_cert, base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &cert_b64);
    std::vector<uint8_t> cert_hash(
        crypto::hash::DigestSizeForHashKind(hash_alg_));
    crypto::hash::Hash(hash_alg_, cert_b64, cert_hash);
    std::string hash_b64;
    base::Base64UrlEncode(cert_hash, base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &hash_b64);
    hash_v.Append(hash_b64);
    pars.Append("");
  }
  sig_d.Set("pars", std::move(pars));
  sig_d.Set("hashV", std::move(hash_v));
  return sig_d;
}

void TwoQwacCertBindingBuilder::GenerateHeader() {
  // Build the minimal JWS header needed for a 2-QWAC TLS certificate binding.
  base::DictValue header = base::DictValue()
                               .Set("alg", SigAlg())
                               .Set("cty", "TLS-Certificate-Binding-v1")
                               .Set("x5c", GenerateX5cHeaderValue())
                               .Set("sigD", GenerateSigDHeaderValue());
  // Add/override values in the header
  header.Merge(header_overrides_.Clone());

  std::string header_string;
  ASSERT_TRUE(JSONStringValueSerializer(&header_string).Serialize(header));
  header_b64_ = std::string();
  base::Base64UrlEncode(
      header_string, base::Base64UrlEncodePolicy::OMIT_PADDING, &*header_b64_);
}

void TwoQwacCertBindingBuilder::GenerateSignature() {
  // All JWS signature algorithms that we support use SHA-256 as their digest.
  const EVP_MD* digest = EVP_sha256();
  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pkey_ctx;
  EVP_PKEY* key = cert_chain_[0]->GetKey();
  ASSERT_TRUE(
      EVP_DigestSignInit(ctx.get(), &pkey_ctx, EVP_sha256(), nullptr, key));
  if (sig_alg_ == JwsSigAlg::kRsaPssSha256) {
    ASSERT_TRUE(EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING));
    ASSERT_TRUE(EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, digest));
    ASSERT_TRUE(EVP_PKEY_CTX_set_rsa_pss_saltlen(
        pkey_ctx, -1 /* match digest and salt length */));
  }

  // The JWS signing input is the the (base64url-encoded) header and payload
  // concatenated and separated by a '.'. For a 2-QWAC cert binding, the
  // payload is always empty.
  const std::string& header = GetHeader();
  const std::array<uint8_t, 1> separator = {'.'};
  ASSERT_TRUE(EVP_DigestSignUpdate(ctx.get(), header.data(), header.size()));
  ASSERT_TRUE(
      EVP_DigestSignUpdate(ctx.get(), separator.data(), separator.size()));
  size_t len = 0;
  std::vector<uint8_t> sig;
  ASSERT_TRUE(EVP_DigestSignFinal(ctx.get(), nullptr, &len));
  sig.resize(len);
  ASSERT_TRUE(EVP_DigestSignFinal(ctx.get(), sig.data(), &len));
  sig.resize(len);
  signature_b64_ = std::string();
  base::Base64UrlEncode(sig, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &*signature_b64_);
}

}  // namespace net

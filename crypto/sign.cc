// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/sign.h"

#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto::sign {

namespace {

bool CanUseKeyForSignatureKind(SignatureKind kind,
                               const crypto::keypair::PrivateKey& key) {
  return true;
}

bool CanUseKeyForSignatureKind(SignatureKind kind,
                               const crypto::keypair::PublicKey& key) {
  return true;
}

const EVP_MD* DigestForSignatureKind(SignatureKind kind) {
  switch (kind) {
    case RSA_PKCS1_SHA1:
      return EVP_sha1();
    case RSA_PKCS1_SHA256:
      return EVP_sha256();
    case RSA_PSS_SHA256:
      return EVP_sha256();
    case ECDSA_SHA256:
      return EVP_sha256();
  }
}

}  // namespace

std::vector<uint8_t> Sign(SignatureKind kind,
                          const crypto::keypair::PrivateKey& key,
                          base::span<const uint8_t> data) {
  Signer signer(kind, key);
  signer.Update(data);
  return signer.Finish();
}

bool Verify(SignatureKind kind,
            const crypto::keypair::PublicKey& key,
            base::span<const uint8_t> data,
            base::span<const uint8_t> signature) {
  Verifier verifier(kind, key, signature);
  verifier.Update(data);
  return verifier.Finish();
}

Signer::Signer(SignatureKind kind, crypto::keypair::PrivateKey key)
    : key_(key), sign_context_(EVP_MD_CTX_new()) {
  CHECK(CanUseKeyForSignatureKind(kind, key));
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_MD* const md = DigestForSignatureKind(kind);
  EVP_PKEY_CTX* pkctx;
  CHECK(
      EVP_DigestSignInit(sign_context_.get(), &pkctx, md, nullptr, key.key()));

  if (kind == RSA_PSS_SHA256) {
    CHECK(EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_PSS_PADDING));
    CHECK(EVP_PKEY_CTX_set_rsa_mgf1_md(pkctx, md));
    // -1 here means "use digest's length"
    CHECK(EVP_PKEY_CTX_set_rsa_pss_saltlen(pkctx, -1));
  }
}
Signer::~Signer() = default;

void Signer::Update(base::span<const uint8_t> data) {
  CHECK(EVP_DigestSignUpdate(sign_context_.get(), data.data(), data.size()));
}

std::vector<uint8_t> Signer::Finish() {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  size_t len = 0;
  // Determine the maximum length of the signature.
  CHECK(EVP_DigestSignFinal(sign_context_.get(), nullptr, &len));

  std::vector<uint8_t> signature(len);
  CHECK(EVP_DigestSignFinal(sign_context_.get(), signature.data(), &len));
  signature.resize(len);
  return signature;
}

Verifier::Verifier(SignatureKind kind,
                   crypto::keypair::PublicKey key,
                   base::span<const uint8_t> signature)
    : key_(key), verify_context_(EVP_MD_CTX_new()) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CHECK(CanUseKeyForSignatureKind(kind, key));
  signature_.resize(signature.size());
  base::span(signature_).copy_from(signature);

  EVP_PKEY_CTX* pkctx;
  const EVP_MD* const md = DigestForSignatureKind(kind);
  CHECK(EVP_DigestVerifyInit(verify_context_.get(), &pkctx, md, nullptr,
                             key.key()));

  if (kind == RSA_PSS_SHA256) {
    CHECK(EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_PSS_PADDING));
    CHECK(EVP_PKEY_CTX_set_rsa_mgf1_md(pkctx, md));
    // -1 here means "use digest's length"
    CHECK(EVP_PKEY_CTX_set_rsa_pss_saltlen(pkctx, -1));
  }
}
Verifier::~Verifier() = default;

void Verifier::Update(base::span<const uint8_t> data) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CHECK_EQ(
      EVP_DigestVerifyUpdate(verify_context_.get(), data.data(), data.size()),
      1);
}

bool Verifier::Finish() {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = EVP_DigestVerifyFinal(verify_context_.get(), signature_.data(),
                                 signature_.size());
  return rv == 1;
}

}  // namespace crypto::sign

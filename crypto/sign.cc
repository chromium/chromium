// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/sign.h"

#include "base/check.h"
#include "base/check_op.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto::sign {

namespace {

enum SignatureMode {
  kOneShot,
  kStreaming,
};

bool CanUseKeyForSignatureKind(SignatureKind kind,
                               const EVP_PKEY* key,
                               SignatureMode mode) {
  // There is a separate EVP_PKEY_RSA_PSS value that this can return for
  // PSS-specific keys, but it's not used by PublicKey or PrivateKey.
  const int id = EVP_PKEY_id(key);
  switch (kind) {
    case RSA_PKCS1_SHA1:
    case RSA_PKCS1_SHA256:
    case RSA_PKCS1_SHA384:
    case RSA_PKCS1_SHA512:
    case RSA_PSS_SHA256:
    case RSA_PSS_SHA384:
    case RSA_PSS_SHA512:
      // There exists an EVP_PKEY_RSA_PSS key type for RSA-PSS-specific keys,
      // but BoringSSL doesn't implement it and Chromium doesn't use it.
      return id == EVP_PKEY_RSA;
    case ECDSA_SHA256:
      return id == EVP_PKEY_EC;
    case ED25519:
      return id == EVP_PKEY_ED25519 && mode == kOneShot;
  }

  return false;
}

const EVP_MD* DigestForSignatureKind(SignatureKind kind) {
  switch (kind) {
    case RSA_PKCS1_SHA1:
      return EVP_sha1();
    case RSA_PKCS1_SHA256:
    case RSA_PSS_SHA256:
    case ECDSA_SHA256:
      return EVP_sha256();
    case RSA_PKCS1_SHA384:
    case RSA_PSS_SHA384:
      return EVP_sha384();
    case RSA_PKCS1_SHA512:
    case RSA_PSS_SHA512:
      return EVP_sha512();
    case ED25519:
      return nullptr;
  }
}

void ConfigurePkeyCtx(EVP_PKEY_CTX* pkctx, SignatureKind kind) {
  if (kind == RSA_PSS_SHA256 || kind == RSA_PSS_SHA384 ||
      kind == RSA_PSS_SHA512) {
    CHECK(EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_PSS_PADDING));
    CHECK(EVP_PKEY_CTX_set_rsa_mgf1_md(pkctx, DigestForSignatureKind(kind)));
    CHECK(EVP_PKEY_CTX_set_rsa_pss_saltlen(pkctx, RSA_PSS_SALTLEN_DIGEST));
  }
}

}  // namespace

std::vector<uint8_t> Sign(SignatureKind kind,
                          const crypto::keypair::PrivateKey& key,
                          base::span<const uint8_t> data) {
  CHECK(CanUseKeyForSignatureKind(kind, key.key(), kOneShot));

  const EVP_MD* const md = DigestForSignatureKind(kind);
  EVP_PKEY_CTX* pkctx;
  bssl::UniquePtr<EVP_MD_CTX> context(EVP_MD_CTX_new());
  CHECK(EVP_DigestSignInit(context.get(), &pkctx, md, nullptr,
                           const_cast<EVP_PKEY*>(key.key())));
  ConfigurePkeyCtx(pkctx, kind);

  size_t len = 0;
  CHECK(EVP_DigestSign(context.get(), nullptr, &len, data.data(), data.size()));
  std::vector<uint8_t> result(len);
  CHECK(EVP_DigestSign(context.get(), result.data(), &len, data.data(),
                       data.size()));
  result.resize(len);
  return result;
}

bool Verify(SignatureKind kind,
            const crypto::keypair::PublicKey& key,
            base::span<const uint8_t> data,
            base::span<const uint8_t> signature) {
  CHECK(CanUseKeyForSignatureKind(kind, key.key(), kOneShot));

  const EVP_MD* const md = DigestForSignatureKind(kind);
  EVP_PKEY_CTX* pkctx;
  bssl::UniquePtr<EVP_MD_CTX> context(EVP_MD_CTX_new());
  CHECK(EVP_DigestVerifyInit(context.get(), &pkctx, md, nullptr,
                             const_cast<EVP_PKEY*>(key.key())));
  ConfigurePkeyCtx(pkctx, kind);

  return EVP_DigestVerify(context.get(), signature.data(), signature.size(),
                          data.data(), data.size()) == 1;
}

Signer::Signer(SignatureKind kind, crypto::keypair::PrivateKey key)
    : key_(key), sign_context_(EVP_MD_CTX_new()) {
  CHECK(CanUseKeyForSignatureKind(kind, key.key(), kStreaming));
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_MD* const md = DigestForSignatureKind(kind);
  EVP_PKEY_CTX* pkctx;
  CHECK(
      EVP_DigestSignInit(sign_context_.get(), &pkctx, md, nullptr, key.key()));
  ConfigurePkeyCtx(pkctx, kind);
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
  CHECK(CanUseKeyForSignatureKind(kind, key.key(), kStreaming));
  signature_.resize(signature.size());
  base::span(signature_).copy_from(signature);

  EVP_PKEY_CTX* pkctx;
  const EVP_MD* const md = DigestForSignatureKind(kind);
  CHECK(EVP_DigestVerifyInit(verify_context_.get(), &pkctx, md, nullptr,
                             key.key()));
  ConfigurePkeyCtx(pkctx, kind);
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

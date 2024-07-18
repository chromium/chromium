// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "crypto/signature_verifier.h"

#include <memory>

#include "base/check_op.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto {

struct SignatureVerifier::VerifyContext {
  bssl::ScopedEVP_MD_CTX ctx;
};

SignatureVerifier::SignatureVerifier() = default;

SignatureVerifier::~SignatureVerifier() = default;

bool SignatureVerifier::VerifyInit(SignatureAlgorithm signature_algorithm,
                                   base::span<const uint8_t> signature,
                                   base::span<const uint8_t> public_key_info) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  int pkey_type = EVP_PKEY_NONE;
  const EVP_MD* digest = nullptr;
  switch (signature_algorithm) {
    case RSA_PKCS1_SHA1:
      pkey_type = EVP_PKEY_RSA;
      digest = EVP_sha1();
      break;
    case RSA_PKCS1_SHA256:
    case RSA_PSS_SHA256:
      pkey_type = EVP_PKEY_RSA;
      digest = EVP_sha256();
      break;
    case ECDSA_SHA256:
      pkey_type = EVP_PKEY_EC;
      digest = EVP_sha256();
      break;
  }
  DCHECK_NE(EVP_PKEY_NONE, pkey_type);
  DCHECK(digest);

  if (verify_context_)
    return false;

  verify_context_ = std::make_unique<VerifyContext>();
  signature_.assign(signature.data(), signature.data() + signature.size());

  CBS cbs;
  CBS_init(&cbs, public_key_info.data(), public_key_info.size());
  bssl::UniquePtr<EVP_PKEY> public_key(EVP_parse_public_key(&cbs));
  if (!public_key || CBS_len(&cbs) != 0 ||
      EVP_PKEY_id(public_key.get()) != pkey_type) {
    return false;
  }

  EVP_PKEY_CTX* pkey_ctx;
  if (!EVP_DigestVerifyInit(verify_context_->ctx.get(), &pkey_ctx, digest,
                            nullptr, public_key.get())) {
    return false;
  }

  if (signature_algorithm == RSA_PSS_SHA256) {
    if (!EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, digest) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(
            pkey_ctx, -1 /* match digest and salt length */)) {
      return false;
    }
  }

  return true;
}

void SignatureVerifier::VerifyUpdate(base::span<const uint8_t> data_part) {
  DCHECK(verify_context_);
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = EVP_DigestVerifyUpdate(verify_context_->ctx.get(), data_part.data(),
                                  data_part.size());
  DCHECK_EQ(rv, 1);
}

bool SignatureVerifier::VerifyFinal() {
  DCHECK(verify_context_);
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = EVP_DigestVerifyFinal(verify_context_->ctx.get(), signature_.data(),
                                 signature_.size());
  DCHECK_EQ(static_cast<int>(!!rv), rv);
  Reset();
  return rv == 1;
}

void SignatureVerifier::Reset() {
  verify_context_.reset();
  signature_.clear();
}

}  // namespace crypto

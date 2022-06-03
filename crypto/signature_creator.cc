// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_creator.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto {

namespace {

const EVP_MD* ToOpenSSLDigest(SignatureCreator::HashAlgorithm hash_alg) {
  switch (hash_alg) {
    case SignatureCreator::SHA1:
      return EVP_sha1();
    case SignatureCreator::SHA256:
      return EVP_sha256();
  }
  return nullptr;
}

int ToOpenSSLDigestType(SignatureCreator::HashAlgorithm hash_alg) {
  switch (hash_alg) {
    case SignatureCreator::SHA1:
      return NID_sha1;
    case SignatureCreator::SHA256:
      return NID_sha256;
  }
  return NID_undef;
}

}  // namespace

SignatureCreator::~SignatureCreator() {
  EVP_MD_CTX_destroy(sign_context_);
}

// static
std::unique_ptr<SignatureCreator> SignatureCreator::Create(
    RSAPrivateKey* key,
    HashAlgorithm hash_alg) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  std::unique_ptr<SignatureCreator> result(new SignatureCreator);
  const EVP_MD* const digest = ToOpenSSLDigest(hash_alg);
  DCHECK(digest);
  if (!digest) {
    return nullptr;
  }
  if (!EVP_DigestSignInit(result->sign_context_, nullptr, digest, nullptr,
                          key->key())) {
    return nullptr;
  }
  return result;
}

// static
bool SignatureCreator::Sign(RSAPrivateKey* key,
                            HashAlgorithm hash_alg,
                            const uint8_t* data,
                            int data_len,
                            std::vector<uint8_t>* signature) {
  bssl::UniquePtr<RSA> rsa_key(EVP_PKEY_get1_RSA(key->key()));
  if (!rsa_key)
    return false;
  signature->resize(RSA_size(rsa_key.get()));

  unsigned int len = 0;
  if (!RSA_sign(ToOpenSSLDigestType(hash_alg), data, data_len,
                signature->data(), &len, rsa_key.get())) {
    signature->clear();
    return false;
  }
  signature->resize(len);
  return true;
}

bool SignatureCreator::Update(const uint8_t* data_part, int data_part_len) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  return !!EVP_DigestSignUpdate(sign_context_, data_part, data_part_len);
}

bool SignatureCreator::Final(std::vector<uint8_t>* signature) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Determine the maximum length of the signature.
  size_t len = 0;
  if (!EVP_DigestSignFinal(sign_context_, nullptr, &len)) {
    signature->clear();
    return false;
  }
  signature->resize(len);

  // Sign it.
  if (!EVP_DigestSignFinal(sign_context_, signature->data(), &len)) {
    signature->clear();
    return false;
  }
  signature->resize(len);
  return true;
}

SignatureCreator::SignatureCreator() : sign_context_(EVP_MD_CTX_create()) {}

}  // namespace crypto

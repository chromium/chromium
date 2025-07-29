// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/rsa_private_key.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto {

// static
std::unique_ptr<RSAPrivateKey> RSAPrivateKey::CreateFromPrivateKeyInfo(
    base::span<const uint8_t> input) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, input.data(), input.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_private_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0 || EVP_PKEY_id(pkey.get()) != EVP_PKEY_RSA)
    return nullptr;

  std::unique_ptr<RSAPrivateKey> result(new RSAPrivateKey);
  result->key_ = std::move(pkey);
  return result;
}

RSAPrivateKey::RSAPrivateKey() = default;

RSAPrivateKey::~RSAPrivateKey() = default;

std::unique_ptr<RSAPrivateKey> RSAPrivateKey::Copy() const {
  std::unique_ptr<RSAPrivateKey> copy(new RSAPrivateKey);
  bssl::UniquePtr<RSA> rsa(EVP_PKEY_get1_RSA(key_.get()));
  if (!rsa)
    return nullptr;
  copy->key_.reset(EVP_PKEY_new());
  if (!EVP_PKEY_set1_RSA(copy->key_.get(), rsa.get()))
    return nullptr;
  return copy;
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8_t>* output) const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  uint8_t *der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_private_key(cbb.get(), key_.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  output->assign(der, UNSAFE_TODO(der + der_len));
  OPENSSL_free(der);
  return true;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8_t>* output) const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  uint8_t *der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), key_.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  output->assign(der, UNSAFE_TODO(der + der_len));
  OPENSSL_free(der);
  return true;
}

}  // namespace crypto

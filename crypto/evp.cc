// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/evp.h"

#include "base/containers/to_vector.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace {

base::span<const uint8_t> CBBToSpan(CBB* cbb) {
  // SAFETY: BoringSSL ensures that CBBs always have CBB_len() bytes available
  // at their CBB_data() pointer.
  UNSAFE_BUFFERS(return base::span<const uint8_t>(CBB_data(cbb), CBB_len(cbb));)
}

}  // namespace

namespace crypto::evp {

bssl::UniquePtr<EVP_PKEY> PublicKeyFromBytes(base::span<const uint8_t> bytes) {
  CBS cbs;
  CBS_init(&cbs, bytes.data(), bytes.size());
  bssl::UniquePtr<EVP_PKEY> key(EVP_parse_public_key(&cbs));
  if (CBS_len(&cbs) != 0) {
    key.reset();
  }
  return key;
}

bssl::UniquePtr<EVP_PKEY> PrivateKeyFromBytes(base::span<const uint8_t> bytes) {
  CBS cbs;
  CBS_init(&cbs, bytes.data(), bytes.size());
  bssl::UniquePtr<EVP_PKEY> key(EVP_parse_private_key(&cbs));
  if (CBS_len(&cbs) != 0) {
    key.reset();
  }
  return key;
}

std::vector<uint8_t> PublicKeyToBytes(const EVP_PKEY* key) {
  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), 0));
  CHECK(EVP_marshal_public_key(cbb.get(), key));

  return base::ToVector(CBBToSpan(cbb.get()));
}

std::vector<uint8_t> PrivateKeyToBytes(const EVP_PKEY* key) {
  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), 0));
  CHECK(EVP_marshal_private_key(cbb.get(), key));

  return base::ToVector(CBBToSpan(cbb.get()));
}

}  // namespace crypto::evp

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/kex.h"

#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::kex {

void EcdhP256(const crypto::keypair::PublicKey& theirs,
              const crypto::keypair::PrivateKey& ours,
              base::span<uint8_t, 32> out) {
  CHECK(theirs.IsEcP256());
  CHECK(ours.IsEcP256());

  const EC_KEY* ourkey = EVP_PKEY_get0_EC_KEY(ours.key());
  const EC_KEY* theirkey = EVP_PKEY_get0_EC_KEY(theirs.key());
  const EC_POINT* theirpoint = EC_KEY_get0_public_key(theirkey);

  CHECK(ECDH_compute_key(out.data(), out.size(), theirpoint, ourkey, nullptr) ==
        out.size());
}

}  // namespace crypto::kex

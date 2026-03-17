// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/kex.h"

#include "base/check_op.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::kex {

namespace {

template <size_t N>
void DeriveSharedECDHSecret(const crypto::keypair::PublicKey& theirs,
                            const crypto::keypair::PrivateKey& ours,
                            base::span<uint8_t, N> out) {
  const EC_KEY* ourkey = EVP_PKEY_get0_EC_KEY(ours.key());
  const EC_KEY* theirkey = EVP_PKEY_get0_EC_KEY(theirs.key());
  const EC_POINT* theirpoint = EC_KEY_get0_public_key(theirkey);

  CHECK_EQ(
      ECDH_compute_key(out.data(), out.size(), theirpoint, ourkey, nullptr),
      static_cast<int>(out.size()));
}

}  // namespace

void EcdhP256(const crypto::keypair::PublicKey& theirs,
              const crypto::keypair::PrivateKey& ours,
              base::span<uint8_t, 32> out) {
  CHECK(theirs.IsEcP256());
  CHECK(ours.IsEcP256());

  DeriveSharedECDHSecret(theirs, ours, out);
}

void EcdhP384(const crypto::keypair::PublicKey& theirs,
              const crypto::keypair::PrivateKey& ours,
              base::span<uint8_t, 48> out) {
  CHECK(theirs.IsEcP384());
  CHECK(ours.IsEcP384());

  DeriveSharedECDHSecret(theirs, ours, out);
}

void X25519(const crypto::keypair::PublicKey& theirs,
            const crypto::keypair::PrivateKey& ours,
            base::span<uint8_t, 32> out) {
  CHECK(theirs.IsX25519());
  CHECK(ours.IsX25519());

  std::array<uint8_t, 32> their_raw = theirs.ToX25519PublicKey();
  std::array<uint8_t, 32> our_raw = ours.ToX25519PrivateKey();

  CHECK_EQ(1, ::X25519(out.data(), our_raw.data(), their_raw.data()));
}

}  // namespace crypto::kex

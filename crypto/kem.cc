// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/kem.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::kem {

namespace {

const EVP_KEM* KemToEvpKem(Kem mech) {
  switch (mech) {
    case kMlkem768:
      return EVP_kem_ml_kem_768();
  }
  NOTREACHED();
}

bool CanUsePrivateKeyForKem(Kem mech,
                            const crypto::keypair::PrivateKey& privkey) {
  switch (mech) {
    case kMlkem768:
      return privkey.IsMlkem768();
  }
  NOTREACHED();
}

bool CanUsePublicKeyForKem(Kem mech, const crypto::keypair::PublicKey& pubkey) {
  switch (mech) {
    case kMlkem768:
      return pubkey.IsMlkem768();
  }
  NOTREACHED();
}

}  // namespace

std::optional<std::vector<uint8_t>> Decapsulate(
    Kem mech,
    const crypto::keypair::PrivateKey& privkey,
    base::span<const uint8_t> ciphertext) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CHECK(CanUsePrivateKeyForKem(mech, privkey));

  const EVP_KEM* kem = KemToEvpKem(mech);
  std::vector<uint8_t> secret(EVP_KEM_secret_len(kem));

  if (EVP_KEM_decap(kem, secret.data(), secret.size(), ciphertext.data(),
                    ciphertext.size(), privkey.key()) != 1) {
    return std::nullopt;
  }

  return secret;
}

EncapResult Encapsulate(Kem mech, const crypto::keypair::PublicKey& pubkey) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CHECK(CanUsePublicKeyForKem(mech, pubkey));

  const EVP_KEM* kem = KemToEvpKem(mech);
  EncapResult result;
  result.ciphertext.resize(EVP_KEM_ciphertext_len(kem));
  result.secret.resize(EVP_KEM_secret_len(kem));

  CHECK_EQ(1, EVP_KEM_encap(kem, result.ciphertext.data(),
                            result.ciphertext.size(), result.secret.data(),
                            result.secret.size(), pubkey.key()));
  return result;
}

}  // namespace crypto::kem

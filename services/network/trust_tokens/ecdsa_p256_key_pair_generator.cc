// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/ecdsa_p256_key_pair_generator.h"

#include "crypto/ec_private_key.h"

namespace network {

bool EcdsaP256KeyPairGenerator::Generate(std::string* signing_key_out,
                                         std::string* verification_key_out) {
  std::unique_ptr<crypto::ECPrivateKey> key_pair =
      crypto::ECPrivateKey::Create();
  std::vector<uint8_t> private_key;
  std::string public_key;
  if (!key_pair->ExportPrivateKey(&private_key)) {
    return false;
  }
  if (!key_pair->ExportRawPublicKey(verification_key_out)) {
    return false;
  }
  signing_key_out->assign(private_key.begin(), private_key.end());

  return true;
}

}  // namespace network

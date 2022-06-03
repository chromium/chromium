// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/ed25519_key_pair_generator.h"

#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {

bool Ed25519KeyPairGenerator::Generate(std::string* signing_key_out,
                                       std::string* verification_key_out) {
  signing_key_out->resize(ED25519_PRIVATE_KEY_LEN);
  verification_key_out->resize(ED25519_PUBLIC_KEY_LEN);

  // This can't fail.
  ED25519_keypair(
      base::as_writable_bytes(base::make_span(*verification_key_out)).data(),
      base::as_writable_bytes(base::make_span(*signing_key_out)).data());

  return true;
}

}  // namespace network

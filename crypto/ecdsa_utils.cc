// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ecdsa_utils.h"

#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "crypto/keypair.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto {

std::optional<std::vector<uint8_t>> ConvertEcdsaDerSignatureToRaw(
    const keypair::PublicKey& public_key,
    base::span<const uint8_t> der_signature) {
  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(public_key.key());
  if (!ec_key) {
    return std::nullopt;
  }

  return ConvertEcdsaDerSignatureToRaw(EC_KEY_get0_group(ec_key),
                                       der_signature);
}

std::optional<std::vector<uint8_t>> ConvertEcdsaDerSignatureToRaw(
    const EC_GROUP* group,
    base::span<const uint8_t> der_signature) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (!group) {
    return std::nullopt;
  }

  // Verify that `der_signature` is a valid ECDSA signature.
  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(
      ECDSA_SIG_from_bytes(der_signature.data(), der_signature.size()));
  if (!ecdsa_sig) {
    return std::nullopt;
  }

  size_t order_size_bits = EC_GROUP_order_bits(group);
  size_t order_size_bytes = (order_size_bits + 7) / 8;

  // Produce r || s output from the ECDSA signature.
  std::vector<uint8_t> raw_signature(2 * order_size_bytes);
  if (!BN_bn2bin_padded(&raw_signature[0], order_size_bytes, ecdsa_sig->r) ||
      !BN_bn2bin_padded(&raw_signature[order_size_bytes], order_size_bytes,
                        ecdsa_sig->s)) {
    return std::nullopt;
  }

  return raw_signature;
}

}  // namespace crypto

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_UTIL_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_UTIL_H_

#include <string>
#include <vector>

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/boringssl/src/include/openssl/base.h"

// This file contains miscellaneous helpers that don't belong in any of the
// other *_util.h

namespace webcrypto {

class Status;

// Returns the EVP_MD that corresponds with |hash_algorithm|, or nullptr on
// failure.
const EVP_MD* GetDigest(const blink::WebCryptoAlgorithm& hash_algorithm);

// Returns the EVP_MD that corresponds with |id|, or nullptr on failure.
const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id);

// Truncates an octet string to a particular bit length. This is accomplished by
// resizing to the closest byte length, and then zero-ing the unused
// least-significant bits of the final byte.
//
// It is an error to call this function with a bit length that is larger than
// that of |bytes|.
//
// TODO(eroman): This operation is not yet defined by the WebCrypto spec,
// however this is a reasonable interpretation:
// https://www.w3.org/Bugs/Public/show_bug.cgi?id=27402
void TruncateToBitLength(size_t length_bits, std::vector<uint8_t>* bytes);

// Rounds a bit count (up) to the nearest byte count.
//
// This is mathematically equivalent to (x + 7) / 8, however has no
// possibility of integer overflow.
template <typename T>
T NumBitsToBytes(T x) {
  return (x / 8) + (7 + (x % 8)) / 8;
}

// Verifies whether a key can be created using |actual_usages| when the
// algorithm supports |all_possible_usages|.
Status CheckKeyCreationUsages(blink::WebCryptoKeyUsageMask all_possible_usages,
                              blink::WebCryptoKeyUsageMask actual_usages);

// TODO(eroman): This doesn't really belong in this file. Move it into Blink
// instead.
//
// Returns true if the set bits in b make up a subset of the set bits in a.
bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b);

// The values of these constants correspond with the "enc" parameter of
// EVP_CipherInit_ex(), do not change.
enum EncryptOrDecrypt { DECRYPT = 0, ENCRYPT = 1 };

// Does either encryption or decryption for an AEAD algorithm.
//   * |mode| controls whether encryption or decryption is done
//   * |aead_alg| the algorithm (for instance AES-GCM)
//   * |buffer| where the ciphertext or plaintext is written to.
Status AeadEncryptDecrypt(EncryptOrDecrypt mode,
                          base::span<const uint8_t> raw_key,
                          base::span<const uint8_t> data,
                          unsigned int tag_length_bytes,
                          base::span<const uint8_t> iv,
                          base::span<const uint8_t> additional_data,
                          const EVP_AEAD* aead_alg,
                          std::vector<uint8_t>* buffer);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_UTIL_H_

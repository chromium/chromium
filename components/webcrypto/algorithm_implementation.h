// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATION_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATION_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

class GenerateKeyResult;
class Status;

// AlgorithmImplementation is a base class for *executing* the operations of an
// algorithm (generating keys, encrypting, signing, etc.).
//
// This is in contrast to blink::WebCryptoAlgorithm which instead *describes*
// the operation and its parameters.
//
// AlgorithmImplementation has reasonable default implementations for all
// methods which behave as if the operation is it is unsupported, so
// implementations need only override the applicable methods.
//
// Unless stated otherwise methods of AlgorithmImplementation are responsible
// for sanitizing their inputs. The following can be assumed:
//
//   * |algorithm.id()| and |key.algorithm.id()| matches the algorithm under
//     which the implementation was registerd.
//   * |algorithm| has the correct parameters type for the operation.
//   * The key usages have already been verified. In fact in the case of calls
//     to Encrypt()/Decrypt() the corresponding key usages may not be present
//     (when wrapping/unwrapping).
class AlgorithmImplementation {
 public:
  virtual ~AlgorithmImplementation();

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the encrypt operation"
  //
  // (crypto.subtle.encrypt() dispatches to this)
  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         base::span<const uint8_t> data,
                         std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the decrypt operation"
  //
  // (crypto.subtle.decrypt() dispatches to this)
  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         base::span<const uint8_t> data,
                         std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the sign operation"
  //
  // (crypto.subtle.sign() dispatches to this)
  virtual Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      base::span<const uint8_t> data,
                      std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the verify operation"
  //
  // (crypto.subtle.verify() dispatches to this)
  virtual Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        base::span<const uint8_t> signature,
                        base::span<const uint8_t> data,
                        bool* signature_match) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the digest operation"
  //
  // (crypto.subtle.digest() dispatches to this)
  virtual Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                        base::span<const uint8_t> data,
                        std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of executing the generate key operation"
  //
  // (crypto.subtle.generateKey() dispatches to this)
  virtual Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                             bool extractable,
                             blink::WebCryptoKeyUsageMask usages,
                             GenerateKeyResult* result) const;

  // This is what is run whenever the spec says:
  //    "Let result be a new ArrayBuffer containing the result of executing the
  //    derive bits operation"
  //
  // (crypto.subtle.deriveBits() dispatches to this)
  virtual Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& base_key,
                            std::optional<unsigned int> length_bits,
                            std::vector<uint8_t>* derived_bytes) const;

  // This is what is run whenever the spec says:
  //    "Let length be the result of executing the get key length algorithm"
  //
  // In the Web Crypto spec the operation returns either "null" or an
  // "Integer". In this code "null" is represented with |std::nullopt|.
  virtual Status GetKeyLength(
      const blink::WebCryptoAlgorithm& key_length_algorithm,
      std::optional<unsigned int>* length_bits) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the import key operation"
  //
  // (crypto.subtle.importKey() dispatches to this).
  virtual Status ImportKey(blink::WebCryptoKeyFormat format,
                           base::span<const uint8_t> key_data,
                           const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usages,
                           blink::WebCryptoKey* key) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the export key operation"
  //
  // (crypto.subtle.exportKey() dispatches to this).
  virtual Status ExportKey(blink::WebCryptoKeyFormat format,
                           const blink::WebCryptoKey& key,
                           std::vector<uint8_t>* buffer) const;

  // -----------------------------------------------
  // Structured clone
  // -----------------------------------------------

  // The Structured clone methods are used for synchronous serialization /
  // deserialization of a WebCryptoKey.
  //
  // This serialized format is used by Blink to:
  //   * Copy WebCryptoKeys between threads (postMessage to WebWorkers)
  //   * Copy WebCryptoKeys between domains (postMessage)
  //   * Copy WebCryptoKeys within the same domain (postMessage)
  //   * Persist the key to storage (IndexedDB)
  //
  // Implementations of structured cloning must:
  //   * Be threadsafe (structured cloning is called directly on the Blink
  //     thread, in contrast to the other methods of AlgorithmImplementation).
  //   * Use a stable format (a serialized key must forever be de-serializable,
  //     and be able to survive future migrations to crypto libraries)
  //   * Work for all keys (including ones marked as non-extractable).
  //   * Gracefully handle invalid inputs
  //
  // Tests to verify structured cloning are available in:
  //   LayoutTests/crypto/clone-*.html

  // Note that SerializeKeyForClone() is not virtual because all
  // implementations end up doing the same thing.
  Status SerializeKeyForClone(const blink::WebCryptoKey& key,
                              blink::WebVector<uint8_t>* key_data) const;

  // Deserializes key data from Blink (used for structured cloning).
  //
  // The inputs to this function originate from Blink, and may not be
  // consistent or valid. Implementations must return a failure when processing
  // invalid or adversarially constructed inputs.
  //
  // The ONLY guarantee implementations can assume is that |algorithm.id()|
  // corresponds with that which the AlgorithmImplementation was registered
  // under.
  //
  // Implementations must be prepared to handle:
  //
  // * |type| being invalid for this algorithm's key type(s)
  // * |algorithm.params()| being inconsistent with the |algorithm.id()|
  // * |usages| being inconsistent with the key type
  // * |extractable| being inconsistent with the key type
  // * |key_data| containing an incorrect serialized format
  // * Backwards-compatibility: the inputs may have been produced by older
  //   versions of the code.
  virtual Status DeserializeKeyForClone(
      const blink::WebCryptoKeyAlgorithm& algorithm,
      blink::WebCryptoKeyType type,
      bool extractable,
      blink::WebCryptoKeyUsageMask usages,
      base::span<const uint8_t> key_data,
      blink::WebCryptoKey* key) const;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATION_H_

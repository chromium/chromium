// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithm_dispatch.h"

#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithm_implementations.h"
#include "components/webcrypto/algorithm_registry.h"
#include "components/webcrypto/encapsulate_result.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

Status DecryptDontCheckKeyUsage(const blink::WebCryptoAlgorithm& algorithm,
                                const blink::WebCryptoKey& key,
                                base::span<const uint8_t> data,
                                std::vector<uint8_t>* buffer) {
  if (algorithm.Id() != key.Algorithm().Id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  return impl->Decrypt(algorithm, key, data, buffer);
}

Status EncryptDontCheckUsage(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             base::span<const uint8_t> data,
                             std::vector<uint8_t>* buffer) {
  if (algorithm.Id() != key.Algorithm().Id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  return impl->Encrypt(algorithm, key, data, buffer);
}

Status ExportKeyDontCheckExtractability(blink::WebCryptoKeyFormat format,
                                        const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(key.Algorithm().Id(), &impl);
  if (status.IsError())
    return status;

  return impl->ExportKey(format, key, buffer);
}

}  // namespace

Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               base::span<const uint8_t> data,
               std::vector<uint8_t>* buffer) {
  if (!key.KeyUsageAllows(blink::kWebCryptoKeyUsageEncrypt))
    return Status::ErrorUnexpected();
  return EncryptDontCheckUsage(algorithm, key, data, buffer);
}

Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               base::span<const uint8_t> data,
               std::vector<uint8_t>* buffer) {
  if (!key.KeyUsageAllows(blink::kWebCryptoKeyUsageDecrypt))
    return Status::ErrorUnexpected();
  return DecryptDontCheckKeyUsage(algorithm, key, data, buffer);
}

Status Digest(const blink::WebCryptoAlgorithm& algorithm,
              base::span<const uint8_t> data,
              std::vector<uint8_t>* buffer) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  return impl->Digest(algorithm, data, buffer);
}

Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   GenerateKeyResult* result) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  status = impl->GenerateKey(algorithm, extractable, usages, result);
  if (status.IsError())
    return status;

  // The Web Crypto spec says to reject secret and private keys generated with
  // empty usages:
  //
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-generateKey
  //
  // (14.3.6.8):
  // If result is a CryptoKey object:
  //     If the [[type]] internal slot of result is "secret" or "private"
  //     and usages is empty, then throw a SyntaxError.
  //
  // (14.3.6.9)
  // If result is a CryptoKeyPair object:
  //     If the [[usages]] internal slot of the privateKey attribute of
  //     result is the empty sequence, then throw a SyntaxError.
  const blink::WebCryptoKey* key = nullptr;
  if (result->type() == GenerateKeyResult::TYPE_SECRET_KEY)
    key = &result->secret_key();
  if (result->type() == GenerateKeyResult::TYPE_PUBLIC_PRIVATE_KEY_PAIR)
    key = &result->private_key();
  if (key == nullptr)
    return Status::ErrorUnexpected();

  if (key->Usages() == 0) {
    return Status::ErrorCreateKeyEmptyUsages();
  }

  return Status::Success();
}

Status ImportKey(blink::WebCryptoKeyFormat format,
                 base::span<const uint8_t> key_data,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  status =
      impl->ImportKey(format, key_data, algorithm, extractable, usages, key);
  if (status.IsError())
    return status;

  // The Web Crypto spec says to reject secret and private keys imported with
  // empty usages:
  //
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-importKey
  //
  // 14.3.9.9: If the [[type]] internal slot of result is "secret" or "private"
  //           and usages is empty, then throw a SyntaxError.
  if (key->Usages() == 0 &&
      (key->GetType() == blink::kWebCryptoKeyTypeSecret ||
       key->GetType() == blink::kWebCryptoKeyTypePrivate)) {
    return Status::ErrorCreateKeyEmptyUsages();
  }

  return Status::Success();
}

Status ExportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 std::vector<uint8_t>* buffer) {
  if (!key.Extractable())
    return Status::ErrorKeyNotExtractable();
  return ExportKeyDontCheckExtractability(format, key, buffer);
}

Status Sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            base::span<const uint8_t> data,
            std::vector<uint8_t>* buffer) {
  if (!key.KeyUsageAllows(blink::kWebCryptoKeyUsageSign))
    return Status::ErrorUnexpected();
  if (algorithm.Id() != key.Algorithm().Id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  return impl->Sign(algorithm, key, data, buffer);
}

Status Verify(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> data,
              bool* signature_match) {
  if (!key.KeyUsageAllows(blink::kWebCryptoKeyUsageVerify))
    return Status::ErrorUnexpected();
  if (algorithm.Id() != key.Algorithm().Id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  return impl->Verify(algorithm, key, signature, data, signature_match);
}

Status WrapKey(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key_to_wrap,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrapping_algorithm,
               std::vector<uint8_t>* buffer) {
  if (!wrapping_key.KeyUsageAllows(blink::kWebCryptoKeyUsageWrapKey))
    return Status::ErrorUnexpected();

  std::vector<uint8_t> exported_data;
  Status status = ExportKey(format, key_to_wrap, &exported_data);
  if (status.IsError())
    return status;
  return EncryptDontCheckUsage(wrapping_algorithm, wrapping_key, exported_data,
                               buffer);
}

Status UnwrapKey(blink::WebCryptoKeyFormat format,
                 base::span<const uint8_t> wrapped_key_data,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& wrapping_algorithm,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key) {
  if (!wrapping_key.KeyUsageAllows(blink::kWebCryptoKeyUsageUnwrapKey))
    return Status::ErrorUnexpected();
  if (wrapping_algorithm.Id() != wrapping_key.Algorithm().Id())
    return Status::ErrorUnexpected();

  std::vector<uint8_t> buffer;
  Status status = DecryptDontCheckKeyUsage(wrapping_algorithm, wrapping_key,
                                           wrapped_key_data, &buffer);
  if (status.IsError())
    return status;

  // NOTE that returning the details of ImportKey() failures may leak
  // information about the plaintext of the encrypted key (for instance the JWK
  // key_ops). As long as the ImportKey error messages don't describe actual
  // key bytes however this should be OK. For more discussion see
  // http://crbug.com/372040
  return ImportKey(format, buffer, algorithm, extractable, usages, key);
}

Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& base_key,
                  std::optional<unsigned int> length_bits,
                  std::vector<uint8_t>* derived_bytes) {
  if (!base_key.KeyUsageAllows(blink::kWebCryptoKeyUsageDeriveBits))
    return Status::ErrorUnexpected();

  if (algorithm.Id() != base_key.Algorithm().Id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return status;

  return impl->DeriveBits(algorithm, base_key, length_bits, derived_bytes);
}

Status DeriveKey(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& base_key,
                 const blink::WebCryptoAlgorithm& import_algorithm,
                 const blink::WebCryptoAlgorithm& key_length_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* derived_key) {
  if (!base_key.KeyUsageAllows(blink::kWebCryptoKeyUsageDeriveKey))
    return Status::ErrorUnexpected();

  if (algorithm.Id() != base_key.Algorithm().Id())
    return Status::ErrorUnexpected();

  if (import_algorithm.Id() != key_length_algorithm.Id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* import_impl = nullptr;
  Status status =
      GetAlgorithmImplementation(import_algorithm.Id(), &import_impl);
  if (status.IsError())
    return status;

  // Determine how many bits long the derived key should be.
  std::optional<unsigned int> length_bits;
  status = import_impl->GetKeyLength(key_length_algorithm, &length_bits);
  if (status.IsError()) {
    return status;
  }

  // Derive the key bytes.
  const AlgorithmImplementation* derive_impl = nullptr;
  status = GetAlgorithmImplementation(algorithm.Id(), &derive_impl);
  if (status.IsError())
    return status;

  std::vector<uint8_t> derived_bytes;
  status =
      derive_impl->DeriveBits(algorithm, base_key, length_bits, &derived_bytes);
  if (status.IsError()) {
    return status;
  }

  // Create the key using the derived bytes.
  //
  // Use "raw-secret" as the key format.
  //
  // https://wicg.github.io/webcrypto-modern-algos/#subtlecrypto-interface-keyformat
  return ImportKey(blink::kWebCryptoKeyFormatRawSecret, derived_bytes,
                   import_algorithm, extractable, usages, derived_key);
}

Status EncapsulateKey(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& encapsulation_key,
                      const blink::WebCryptoAlgorithm& shared_key_algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      EncapsulateKeyResult* result) {
  if (algorithm.Id() != encapsulation_key.Algorithm().Id()) {
    return Status::ErrorUnexpected();
  }

  // Method described by:
  // https://wicg.github.io/webcrypto-modern-algos/#SubtleCrypto-method-encapsulateKey
  if (!encapsulation_key.KeyUsageAllows(
          blink::kWebCryptoKeyUsageEncapsulateKey)) {
    return Status::ErrorUnexpected();
  }

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError()) {
    return status;
  }

  // 3.2.1.12: Let encapsulatedBits be the result of performing the encapsulate
  //           operation specified by the algorithm internal slot of
  //           encapsulationKey using encapsulationKey.
  //
  // (encapsulatedBits = shared secret + ciphertext
  std::vector<uint8_t> shared_secret;
  std::vector<uint8_t> ciphertext;
  status = impl->Encapsulate(algorithm, encapsulation_key, &shared_secret,
                             &ciphertext);
  if (status.IsError()) {
    return status;
  }

  // 3.2.1.13: Let sharedKey be the result of performing the import key
  //           operation specified by normalizedSharedKeyAlgorithm using
  //           "raw-secret" as format, the sharedKey field of encapsulatedBits
  //           as keyData, sharedKeyAlgorithm as algorithm and using extractable
  //           and usages.
  //
  // 3.2.1.14: Set the extractable internal slot of sharedKey to extractable.
  //
  // 3.2.1.15: Set the usages internal slot of sharedKey to the normalized value
  // of usages.
  blink::WebCryptoKey shared_key;
  status = ImportKey(blink::kWebCryptoKeyFormatRawSecret, shared_secret,
                     shared_key_algorithm, extractable, usages, &shared_key);
  if (status.IsError()) {
    return status;
  }

  result->Assign(shared_key, std::move(ciphertext));
  return Status::Success();
}

Status EncapsulateBits(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& encapsulation_key,
                       EncapsulateBitsResult* result) {
  if (algorithm.Id() != encapsulation_key.Algorithm().Id()) {
    return Status::ErrorUnexpected();
  }
  if (!encapsulation_key.KeyUsageAllows(
          blink::kWebCryptoKeyUsageEncapsulateBits)) {
    return Status::ErrorUnexpected();
  }

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError()) {
    return status;
  }

  std::vector<uint8_t> shared_secret;
  std::vector<uint8_t> ciphertext;
  status = impl->Encapsulate(algorithm, encapsulation_key, &shared_secret,
                             &ciphertext);
  if (status.IsError()) {
    return status;
  }

  result->Assign(std::move(shared_secret), std::move(ciphertext));
  return Status::Success();
}

Status DecapsulateKey(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& decapsulation_key,
                      base::span<const uint8_t> ciphertext,
                      const blink::WebCryptoAlgorithm& shared_key_algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* shared_key) {
  if (algorithm.Id() != decapsulation_key.Algorithm().Id()) {
    return Status::ErrorUnexpected();
  }
  // Method described by:
  // https://wicg.github.io/webcrypto-modern-algos/#SubtleCrypto-method-decapsulateKey

  if (!decapsulation_key.KeyUsageAllows(
          blink::kWebCryptoKeyUsageDecapsulateKey)) {
    return Status::ErrorUnexpected();
  }

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError()) {
    return status;
  }

  // 3.2.3.13: Let decapsulatedBits be the result of performing the decapsulate
  //           operation specified by the algorithm internal slot of
  //           decapsulationKey using decapsulationKey and ciphertext.
  std::vector<uint8_t> shared_secret;
  status = impl->Decapsulate(algorithm, decapsulation_key, ciphertext,
                             &shared_secret);
  if (status.IsError()) {
    return status;
  }

  // 3.2.3.14: Let sharedKey be the result of performing the import key
  //           operation specified by normalizedSharedKeyAlgorithm using
  //           "raw-secret" as format, the decapsulatedBits as keyData,
  //           sharedKeyAlgorithm as algorithm and using extractable and usages.
  //
  // 3.2.3.15: Set the extractable internal slot of sharedKey to extractable.
  //
  // 3.2.3.16: Set the usages internal slot of sharedKey to the normalized value
  //           of usages.
  return ImportKey(blink::kWebCryptoKeyFormatRawSecret, shared_secret,
                   shared_key_algorithm, extractable, usages, shared_key);
}

Status DecapsulateBits(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& decapsulation_key,
                       base::span<const uint8_t> ciphertext,
                       std::vector<uint8_t>* shared_bits) {
  if (algorithm.Id() != decapsulation_key.Algorithm().Id()) {
    return Status::ErrorUnexpected();
  }
  if (!decapsulation_key.KeyUsageAllows(
          blink::kWebCryptoKeyUsageDecapsulateBits)) {
    return Status::ErrorUnexpected();
  }

  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError()) {
    return status;
  }

  return impl->Decapsulate(algorithm, decapsulation_key, ciphertext,
                           shared_bits);
}

bool SerializeKeyForClone(const blink::WebCryptoKey& key,
                          std::vector<uint8_t>* key_data) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(key.Algorithm().Id(), &impl);
  if (status.IsError())
    return false;

  status = impl->SerializeKeyForClone(key, key_data);
  return status.IsSuccess();
}

bool DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                            blink::WebCryptoKeyType type,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usages,
                            base::span<const uint8_t> key_data,
                            blink::WebCryptoKey* key) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError())
    return false;

  status = impl->DeserializeKeyForClone(algorithm, type, extractable, usages,
                                        key_data, key);
  return status.IsSuccess();
}

bool Supports(blink::WebCryptoOperation op,
              const blink::WebCryptoAlgorithm& algorithm,
              std::optional<unsigned int> length_bits) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(algorithm.Id(), &impl);
  if (status.IsError()) {
    return false;
  }

  return impl->Supports(op, algorithm, length_bits);
}

Status GetKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                    std::optional<unsigned int>* length_bits) {
  const AlgorithmImplementation* impl = nullptr;
  Status status = GetAlgorithmImplementation(key_length_algorithm.Id(), &impl);
  if (status.IsError()) {
    return status;
  }

  return impl->GetKeyLength(key_length_algorithm, length_bits);
}

}  // namespace webcrypto

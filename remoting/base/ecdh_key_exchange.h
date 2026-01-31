// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ECDH_KEY_EXCHANGE_H_
#define REMOTING_BASE_ECDH_KEY_EXCHANGE_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"

namespace crypto {
class Aead;
}  // namespace crypto

namespace remoting {

// Used for the ECDH key exchange process which is used to create an
// |AesGcmCrypter| instance that is used to encrypt/decrypt payloads.
// TODO: crbug.com/359620500 - Move to an HPKE based solution.
class EcdhKeyExchange {
 public:
  // Class for AES-GCM encryption/decryption.
  class AesGcmCrypter {
   public:
    AesGcmCrypter(const AesGcmCrypter&) = delete;
    AesGcmCrypter& operator=(const AesGcmCrypter&) = delete;
    ~AesGcmCrypter();

    // Encrypts |plaintext| using AES-GCM with the derived encryption key.
    // Returns the ciphertext with a randomly generated IV prepended or
    // std::nullopt on failure.
    std::optional<std::vector<uint8_t>> Encrypt(
        base::span<const uint8_t> plaintext);

    // Decrypts |ciphertext_iv| using AES-GCM with the derived encryption key.
    // |ciphertext_iv| is expected to have the IV prepended. Returns the
    // decrypted content or std::nullopt on failure.
    std::optional<std::vector<uint8_t>> Decrypt(
        base::span<const uint8_t> ciphertext_iv);

   private:
    friend class EcdhKeyExchange;

    using DerivedKey = std::array<uint8_t, crypto::hash::kSha256Size>;
    explicit AesGcmCrypter(DerivedKey derived_key);

    // The AES-GCM encryption/decryption object.
    std::unique_ptr<crypto::Aead> context_;
  };

  EcdhKeyExchange();

  EcdhKeyExchange(const EcdhKeyExchange&) = delete;
  EcdhKeyExchange& operator=(const EcdhKeyExchange&) = delete;

  ~EcdhKeyExchange();

  // Derives a shared secret from the local private key and the peer's public
  // key, then uses HKDF to derive an AES-GCM 256-bit encryption key and uses it
  // to create an |AesGcmCrypter|. Returns nullptr if an encryption key cannot
  // be derived from |peer_public_key|.
  std::unique_ptr<AesGcmCrypter> CreateAesGcmCrypter(
      base::span<const uint8_t> peer_public_key) const;

  // A Base64 encoded representation of the public key, useful for logging.
  std::string PublicKeyBase64() const;

  // The raw public key, used for the key exchange.
  base::span<const uint8_t> public_key_bytes() const {
    return public_key_bytes_;
  }

 private:
  // The ECDH private key.
  crypto::keypair::PrivateKey private_key_{
      crypto::keypair::PrivateKey::GenerateEcP384()};

  // The raw public key, used for the key exchange.
  std::vector<uint8_t> public_key_bytes_{
      private_key_.ToUncompressedX962Point()};
};

}  // namespace remoting

#endif  // REMOTING_BASE_ECDH_KEY_EXCHANGE_H_

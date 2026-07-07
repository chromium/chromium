// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/pin_internal.h"

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/i18n/char_iterator.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "crypto/hash.h"
#include "crypto/kdf.h"
#include "crypto/kex.h"
#include "crypto/keypair.h"
#include "crypto/random.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {
namespace pin {

// ProtocolV1 implements CTAP2.1 PIN/UV Auth Protocol One (6.5.10).
class ProtocolV1 : public Protocol {
 private:
  static constexpr size_t kSharedKeySize = 32u;
  static constexpr size_t kSignatureSize = 16u;

  std::array<uint8_t, kP256X962Length> Encapsulate(
      const KeyAgreementResponse& peers_key,
      std::vector<uint8_t>* out_shared_key) const override {
    const auto key = crypto::keypair::PrivateKey::GenerateEcP256();
    *out_shared_key = CalculateSharedKey(key, peers_key.key);
    std::array<uint8_t, kP256X962Length> x962;
    base::span(x962).copy_from(key.ToUncompressedX962Point());
    return x962;
  }

  std::vector<uint8_t> Encrypt(
      base::span<const uint8_t> shared_key,
      base::span<const uint8_t> plaintext) const override {
    DCHECK_EQ(plaintext.size() % AES_BLOCK_SIZE, 0u);
    DCHECK_EQ(shared_key.size(), kSharedKeySize);

    std::vector<uint8_t> ciphertext(plaintext.size());

    EVP_CIPHER_CTX aes_ctx;
    EVP_CIPHER_CTX_init(&aes_ctx);
    const uint8_t kZeroIV[AES_BLOCK_SIZE] = {};
    CHECK(EVP_EncryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr,
                             shared_key.data(), kZeroIV));
    CHECK(EVP_CIPHER_CTX_set_padding(&aes_ctx, 0 /* no padding */));
    CHECK(EVP_Cipher(&aes_ctx, ciphertext.data(), plaintext.data(),
                     plaintext.size()));
    EVP_CIPHER_CTX_cleanup(&aes_ctx);
    return ciphertext;
  }

  std::vector<uint8_t> Decrypt(
      base::span<const uint8_t> shared_key,
      base::span<const uint8_t> ciphertext) const override {
    DCHECK_EQ(ciphertext.size() % AES_BLOCK_SIZE, 0u);
    DCHECK_EQ(shared_key.size(), kSharedKeySize);

    std::vector<uint8_t> plaintext(ciphertext.size());

    EVP_CIPHER_CTX aes_ctx;
    EVP_CIPHER_CTX_init(&aes_ctx);
    const uint8_t kZeroIV[AES_BLOCK_SIZE] = {};
    CHECK(EVP_DecryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr,
                             shared_key.data(), kZeroIV));
    CHECK(EVP_CIPHER_CTX_set_padding(&aes_ctx, 0 /* no padding */));

    CHECK(EVP_Cipher(&aes_ctx, plaintext.data(), ciphertext.data(),
                     ciphertext.size()));
    EVP_CIPHER_CTX_cleanup(&aes_ctx);
    return plaintext;
  }

  std::vector<uint8_t> Authenticate(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) const override {
    // Authenticate can be invoked with the shared secret or with a PIN/UV Auth
    // Token. In CTAP2.1, V1 tokens are fixed at 16 or 32 bytes. But in CTAP2.0
    // they may be any multiple of 16 bytes. We don't know the CTAP version, so
    // only enforce the latter.
    static_assert(kSharedKeySize == 32u, "");
    DCHECK_EQ(key.size() % AES_BLOCK_SIZE, 0u);

    std::vector<uint8_t> pin_auth(SHA256_DIGEST_LENGTH);
    unsigned hmac_bytes;
    CHECK(HMAC(EVP_sha256(), key.data(), key.size(), data.data(), data.size(),
               pin_auth.data(), &hmac_bytes));
    DCHECK_EQ(pin_auth.size(), static_cast<size_t>(hmac_bytes));
    pin_auth.resize(kSignatureSize);
    return pin_auth;
  }

  bool Verify(base::span<const uint8_t> key,
              base::span<const uint8_t> data,
              base::span<const uint8_t> signature) const override {
    if (signature.size() != kSignatureSize) {
      return false;
    }
    const std::vector<uint8_t> computed_signature = Authenticate(key, data);
    CHECK_EQ(computed_signature.size(), kSignatureSize);
    return CRYPTO_memcmp(signature.data(), computed_signature.data(),
                         kSignatureSize) == 0;
  }

  std::vector<uint8_t> CalculateSharedKey(
      crypto::keypair::PrivateKey ours,
      crypto::keypair::PublicKey theirs) const override {
    std::array<uint8_t, 32> shared_value;
    crypto::kex::EcdhP256(theirs, ours, shared_value);
    return base::ToVector(crypto::hash::Sha256(shared_value));
  }
};

// ProtocolV2 implements CTAP2.1 PIN/UV Auth Protocol Two (6.5.11).
class ProtocolV2 : public ProtocolV1 {
 private:
  static constexpr size_t kAESKeyLength = 32;
  static constexpr size_t kHMACKeyLength = 32;
  static constexpr size_t kSharedKeyLength = kAESKeyLength + kHMACKeyLength;
  static constexpr size_t kPINUVAuthTokenLength = 32;
  static constexpr size_t kSignatureSize = SHA256_DIGEST_LENGTH;

  // GetHMACSubKey returns the HMAC-key portion of the shared secret.
  static base::span<const uint8_t, kHMACKeyLength> GetHMACSubKey(
      base::span<const uint8_t, kSharedKeyLength> shared_key) {
    return shared_key.first<kHMACKeyLength>();
  }

  // GetAESSubKey returns the HMAC-key portion of the shared secret.
  static base::span<const uint8_t, kAESKeyLength> GetAESSubKey(
      base::span<const uint8_t, kSharedKeyLength> shared_key) {
    return shared_key.last<kAESKeyLength>();
  }

  std::vector<uint8_t> Encrypt(
      base::span<const uint8_t> shared_key,
      base::span<const uint8_t> plaintext) const override {
    DCHECK_EQ(plaintext.size() % AES_BLOCK_SIZE, 0u);

    const base::span<const uint8_t, kAESKeyLength> aes_key =
        GetAESSubKey(*shared_key.to_fixed_extent<kSharedKeyLength>());

    std::vector<uint8_t> result(AES_BLOCK_SIZE + plaintext.size());
    const auto [iv, ciphertext] = base::span(result).split_at<AES_BLOCK_SIZE>();

    crypto::RandBytes(iv);

    EVP_CIPHER_CTX aes_ctx;
    EVP_CIPHER_CTX_init(&aes_ctx);
    CHECK(EVP_EncryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr,
                             aes_key.data(), iv.data()));
    CHECK(EVP_CIPHER_CTX_set_padding(&aes_ctx, 0 /* no padding */));
    CHECK(EVP_Cipher(&aes_ctx, ciphertext.data(), plaintext.data(),
                     plaintext.size()));
    EVP_CIPHER_CTX_cleanup(&aes_ctx);

    return result;
  }

  std::vector<uint8_t> Decrypt(base::span<const uint8_t> shared_key,
                               base::span<const uint8_t> input) const override {
    DCHECK_EQ(input.size() % AES_BLOCK_SIZE, 0u);

    const base::span<const uint8_t, kAESKeyLength> aes_key =
        GetAESSubKey(*shared_key.to_fixed_extent<kSharedKeyLength>());
    const auto [iv, ciphertext] = input.split_at<AES_BLOCK_SIZE>();
    std::vector<uint8_t> plaintext(ciphertext.size());

    EVP_CIPHER_CTX aes_ctx;
    EVP_CIPHER_CTX_init(&aes_ctx);
    CHECK(EVP_DecryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr,
                             aes_key.data(), iv.data()));
    CHECK(EVP_CIPHER_CTX_set_padding(&aes_ctx, 0 /* no padding */));

    CHECK(EVP_Cipher(&aes_ctx, plaintext.data(), ciphertext.data(),
                     ciphertext.size()));
    EVP_CIPHER_CTX_cleanup(&aes_ctx);

    return plaintext;
  }

  std::vector<uint8_t> Authenticate(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) const override {
    // Authenticate can be invoked with the shared secret or with a PIN/UV Auth
    // Token, which is fixed at 32 bytes in V2.
    DCHECK(key.size() == kSharedKeyLength ||
           key.size() == kPINUVAuthTokenLength);
    const base::span<const uint8_t, kHMACKeyLength> hmac_key =
        (key.size() == kSharedKeyLength
             ? GetHMACSubKey(*key.to_fixed_extent<kSharedKeyLength>())
             : *key.to_fixed_extent<kPINUVAuthTokenLength>());

    std::vector<uint8_t> pin_auth(SHA256_DIGEST_LENGTH);
    unsigned hmac_bytes;
    CHECK(HMAC(EVP_sha256(), hmac_key.data(), hmac_key.size(), data.data(),
               data.size(), pin_auth.data(), &hmac_bytes));
    DCHECK_EQ(pin_auth.size(), static_cast<size_t>(hmac_bytes));
    return pin_auth;
  }

  bool Verify(base::span<const uint8_t> key,
              base::span<const uint8_t> data,
              base::span<const uint8_t> signature) const override {
    if (signature.size() != kSignatureSize) {
      return false;
    }
    const std::vector<uint8_t> computed_signature = Authenticate(key, data);
    CHECK_EQ(computed_signature.size(), kSignatureSize);
    return CRYPTO_memcmp(signature.data(), computed_signature.data(),
                         kSignatureSize) == 0;
  }

  std::vector<uint8_t> CalculateSharedKey(
      crypto::keypair::PrivateKey ours,
      crypto::keypair::PublicKey theirs) const override {
    std::array<uint8_t, 32> shared_value;
    crypto::kex::EcdhP256(theirs, ours, shared_value);

    std::vector<uint8_t> shared_key(kSharedKeyLength);
    const auto [hmac_key, aes_key] =
        base::span(shared_key).split_at<crypto::hash::kSha256Size>();
    constexpr std::string_view kHMACKeyInfo = "CTAP2 HMAC key";
    constexpr std::string_view kAESKeyInfo = "CTAP2 AES key";
    crypto::kdf::Hkdf(crypto::hash::kSha256, shared_value, /*salt=*/{},
                      base::as_byte_span(kHMACKeyInfo), hmac_key);
    crypto::kdf::Hkdf(crypto::hash::kSha256, shared_value, /*salt=*/{},
                      base::as_byte_span(kAESKeyInfo), aes_key);
    return shared_key;
  }
};

// static
const Protocol& ProtocolVersion(PINUVAuthProtocol protocol) {
  static const base::NoDestructor<ProtocolV1> kProtocolV1;
  static const base::NoDestructor<ProtocolV2> kProtocolV2;

  switch (protocol) {
    case PINUVAuthProtocol::kV1:
      return *kProtocolV1;
    case PINUVAuthProtocol::kV2:
      return *kProtocolV2;
  }
}

}  // namespace pin

}  // namespace device

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/pin_internal.h"

#include <string>
#include <utility>

#include "base/i18n/char_iterator.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "crypto/random.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {
namespace pin {

std::optional<bssl::UniquePtr<EC_POINT>> PointFromKeyAgreementResponse(
    const EC_GROUP* group,
    const KeyAgreementResponse& response) {
  bssl::UniquePtr<EC_POINT> ret(EC_POINT_new(group));

  bssl::UniquePtr<BIGNUM> x_bn(BN_new()), y_bn(BN_new());
  BN_bin2bn(response.x, sizeof(response.x), x_bn.get());
  BN_bin2bn(response.y, sizeof(response.y), y_bn.get());
  const bool on_curve =
      EC_POINT_set_affine_coordinates_GFp(group, ret.get(), x_bn.get(),
                                          y_bn.get(), nullptr /* ctx */) == 1;

  if (!on_curve) {
    return std::nullopt;
  }

  return ret;
}

// ProtocolV1 implements CTAP2.1 PIN/UV Auth Protocol One (6.5.10).
class ProtocolV1 : public Protocol {
 private:
  static constexpr size_t kSharedKeySize = 32u;
  static constexpr size_t kSignatureSize = 16u;

  std::array<uint8_t, kP256X962Length> Encapsulate(
      const KeyAgreementResponse& peers_key,
      std::vector<uint8_t>* out_shared_key) const override {
    bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    CHECK(EC_KEY_generate_key(key.get()));
    std::optional<bssl::UniquePtr<EC_POINT>> peers_point =
        PointFromKeyAgreementResponse(EC_KEY_get0_group(key.get()), peers_key);
    *out_shared_key = CalculateSharedKey(key.get(), peers_point->get());
    // KeyAgreementResponse parsing ensures that the point is on the curve.
    DCHECK(peers_point);
    std::array<uint8_t, kP256X962Length> x962;
    CHECK_EQ(x962.size(),
             EC_POINT_point2oct(EC_KEY_get0_group(key.get()),
                                EC_KEY_get0_public_key(key.get()),
                                POINT_CONVERSION_UNCOMPRESSED, x962.data(),
                                x962.size(), nullptr /* BN_CTX */));

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
      const EC_KEY* key,
      const EC_POINT* peers_key) const override {
    std::vector<uint8_t> shared_key(SHA256_DIGEST_LENGTH);
    CHECK_EQ(static_cast<int>(SHA256_DIGEST_LENGTH),
             ECDH_compute_key(shared_key.data(), shared_key.size(), peers_key,
                              key, SHA256KDF));
    return shared_key;
  }

  static void* SHA256KDF(const void* in,
                         size_t in_len,
                         void* out,
                         size_t* out_len) {
    DCHECK_GE(*out_len, static_cast<size_t>(SHA256_DIGEST_LENGTH));
    SHA256(reinterpret_cast<const uint8_t*>(in), in_len,
           reinterpret_cast<uint8_t*>(out));
    *out_len = SHA256_DIGEST_LENGTH;
    return out;
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
    const base::span<uint8_t> iv =
        base::make_span(result).first<AES_BLOCK_SIZE>();
    const base::span<uint8_t> ciphertext =
        base::make_span(result).subspan<AES_BLOCK_SIZE>();

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
    const base::span<const uint8_t> iv = input.first<AES_BLOCK_SIZE>();
    const base::span<const uint8_t> ciphertext =
        input.subspan<AES_BLOCK_SIZE>();
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
      const EC_KEY* key,
      const EC_POINT* peers_key) const override {
    std::vector<uint8_t> shared_key(kSharedKeyLength);
    CHECK_EQ(static_cast<int>(kSharedKeyLength),
             ECDH_compute_key(shared_key.data(), shared_key.size(), peers_key,
                              key, KDF));
    return shared_key;
  }

  static void* KDF(const void* in, size_t in_len, void* out, size_t* out_len) {
    static_assert(kSharedKeyLength == 2 * SHA256_DIGEST_LENGTH, "");
    DCHECK_GE(*out_len, static_cast<size_t>(kSharedKeyLength));
    auto hmac_key_out = base::make_span(
        static_cast<uint8_t*>(out), static_cast<size_t>(SHA256_DIGEST_LENGTH));
    auto aes_key_out =
        base::make_span(static_cast<uint8_t*>(out) + SHA256_DIGEST_LENGTH,
                        static_cast<size_t>(SHA256_DIGEST_LENGTH));

    constexpr uint8_t kHMACKeyInfo[] = "CTAP2 HMAC key";
    constexpr uint8_t kAESKeyInfo[] = "CTAP2 AES key";
    constexpr uint8_t kZeroSalt[32] = {};

    CHECK(HKDF(hmac_key_out.data(), hmac_key_out.size(), EVP_sha256(),
               reinterpret_cast<const uint8_t*>(in), in_len, kZeroSalt,
               sizeof(kZeroSalt), kHMACKeyInfo, sizeof(kHMACKeyInfo) - 1));
    CHECK(HKDF(aes_key_out.data(), aes_key_out.size(), EVP_sha256(),
               reinterpret_cast<const uint8_t*>(in), in_len, kZeroSalt,
               sizeof(kZeroSalt), kAESKeyInfo, sizeof(kAESKeyInfo) - 1));

    *out_len = kSharedKeyLength;
    return out;
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

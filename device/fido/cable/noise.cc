// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/cable/noise.h"

#include <string.h>

#include "base/check_op.h"
#include "base/numerics/byte_conversions.h"
#include "crypto/aead.h"
#include "crypto/sha2.h"
#include "device/fido/fido_constants.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace {

// HKDF2 implements the functions with the same name from Noise[1],
// specialized to the case where |num_outputs| is two.
//
// [1] https://www.noiseprotocol.org/noise.html#hash-functions
std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> HKDF2(
    base::span<const uint8_t, 32> ck,
    base::span<const uint8_t> ikm) {
  uint8_t output[32 * 2];
  HKDF(output, sizeof(output), EVP_sha256(), ikm.data(), ikm.size(), ck.data(),
       ck.size(), /*info=*/nullptr, 0);

  std::array<uint8_t, 32> a, b;
  memcpy(a.data(), &output[0], 32);
  memcpy(b.data(), &output[32], 32);

  return std::make_tuple(a, b);
}

}  // namespace

namespace device {

Noise::Noise() = default;
Noise::~Noise() = default;

void Noise::Init(Noise::HandshakeType type) {
  // See https://www.noiseprotocol.org/noise.html#the-handshakestate-object
  static const char kKNProtocolName[] = "Noise_KNpsk0_P256_AESGCM_SHA256";
  static const char kNKProtocolName[] = "Noise_NKpsk0_P256_AESGCM_SHA256";
  static const char kNKNoPskProtocolName[] = "Noise_NK_P256_AESGCM_SHA256";
  static_assert(sizeof(kNKProtocolName) == sizeof(kKNProtocolName),
                "protocol names are different lengths");
  static_assert(sizeof(kKNProtocolName) == crypto::kSHA256Length,
                "name may need padding if not HASHLEN bytes long");
  static_assert(
      std::tuple_size<decltype(chaining_key_)>::value == crypto::kSHA256Length,
      "chaining_key_ is wrong size");
  static_assert(std::tuple_size<decltype(h_)>::value == crypto::kSHA256Length,
                "h_ is wrong size");

  switch (type) {
    case HandshakeType::kNKpsk0:
      memcpy(chaining_key_.data(), kNKProtocolName, sizeof(kNKProtocolName));
      break;

    case HandshakeType::kKNpsk0:
      memcpy(chaining_key_.data(), kKNProtocolName, sizeof(kKNProtocolName));
      break;

    case HandshakeType::kNK:
      memset(chaining_key_.data(), 0, chaining_key_.size());
      memcpy(chaining_key_.data(), kNKNoPskProtocolName,
             sizeof(kNKNoPskProtocolName));
      break;
  }

  h_ = chaining_key_;
}

void Noise::MixHash(base::span<const uint8_t> in) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, h_.data(), h_.size());
  SHA256_Update(&ctx, in.data(), in.size());
  SHA256_Final(h_.data(), &ctx);
}

void Noise::MixKey(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  std::array<uint8_t, 32> temp_k;
  std::tie(chaining_key_, temp_k) = HKDF2(chaining_key_, ikm);
  InitializeKey(temp_k);
}

void Noise::MixKeyAndHash(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  uint8_t output[32 * 3];
  HKDF(output, sizeof(output), EVP_sha256(), ikm.data(), ikm.size(),
       chaining_key_.data(), chaining_key_.size(), /*info=*/nullptr, 0);
  DCHECK_EQ(chaining_key_.size(), 32u);
  memcpy(chaining_key_.data(), output, 32);
  MixHash(base::span<const uint8_t>(&output[32], 32u));
  InitializeKey(base::span<const uint8_t, 32>(&output[64], 32u));
}

std::vector<uint8_t> Noise::EncryptAndHash(
    base::span<const uint8_t> plaintext) {
  uint8_t nonce[12] = {};
  base::span(nonce).first<4u>().copy_from(
      base::numerics::U32ToBigEndian(symmetric_nonce_));
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  std::vector<uint8_t> ciphertext = aead.Seal(plaintext, nonce, h_);
  MixHash(ciphertext);
  return ciphertext;
}

std::optional<std::vector<uint8_t>> Noise::DecryptAndHash(
    base::span<const uint8_t> ciphertext) {
  uint8_t nonce[12] = {};
  base::span(nonce).first<4u>().copy_from(
      base::numerics::U32ToBigEndian(symmetric_nonce_));
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  auto plaintext = aead.Open(ciphertext, nonce, h_);
  if (plaintext) {
    MixHash(ciphertext);
  }
  return plaintext;
}

std::array<uint8_t, 32> Noise::handshake_hash() const {
  return h_;
}

void Noise::MixHashPoint(const EC_POINT* point) {
  uint8_t x962[kP256X962Length];
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK_EQ(sizeof(x962),
           EC_POINT_point2oct(p256.get(), point, POINT_CONVERSION_UNCOMPRESSED,
                              x962, sizeof(x962), /*ctx=*/nullptr));
  MixHash(x962);
}

std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>>
Noise::traffic_keys() const {
  return HKDF2(chaining_key_, base::span<const uint8_t>());
}

void Noise::InitializeKey(base::span<const uint8_t, 32> key) {
  // See https://www.noiseprotocol.org/noise.html#the-cipherstate-object
  DCHECK_EQ(symmetric_key_.size(), key.size());
  memcpy(symmetric_key_.data(), key.data(), symmetric_key_.size());
  symmetric_nonce_ = 0;
}

}  // namespace device

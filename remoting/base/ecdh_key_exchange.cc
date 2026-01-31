// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/ecdh_key_exchange.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "crypto/aead.h"
#include "crypto/hash.h"
#include "crypto/kdf.h"
#include "crypto/kex.h"
#include "crypto/keypair.h"
#include "crypto/random.h"

namespace remoting {

namespace {
constexpr int kEcdhP384SizeBytes = 48;
constexpr size_t kAesGcmIvSizeBytes = 12;
constexpr char kHkdfInfo[] = "Chromoting ECDH AES Crypter";

}  // namespace

EcdhKeyExchange::AesGcmCrypter::AesGcmCrypter(DerivedKey derived_key)
    : context_(std::make_unique<crypto::Aead>(crypto::Aead::AES_256_GCM,
                                              std::move(derived_key))) {}
EcdhKeyExchange::AesGcmCrypter::~AesGcmCrypter() = default;

EcdhKeyExchange::EcdhKeyExchange() = default;
EcdhKeyExchange::~EcdhKeyExchange() = default;

std::string EcdhKeyExchange::PublicKeyBase64() const {
  return base::Base64Encode(public_key_bytes_);
}

std::unique_ptr<EcdhKeyExchange::AesGcmCrypter>
EcdhKeyExchange::CreateAesGcmCrypter(
    base::span<const uint8_t> peer_public_key_bytes) const {
  std::optional<crypto::keypair::PublicKey> peer_public_key =
      crypto::keypair::PublicKey::FromEcP384Point(peer_public_key_bytes);
  if (!peer_public_key.has_value()) {
    LOG(ERROR) << "Failed to create public key from peer public key bytes ";
    return nullptr;
  }

  std::array<uint8_t, kEcdhP384SizeBytes> shared_secret;
  crypto::kex::EcdhP384(*peer_public_key, private_key_, shared_secret);

  AesGcmCrypter::DerivedKey derived_key;
  crypto::kdf::Hkdf(crypto::hash::HashKind::kSha256, base::span(shared_secret),
                    /* salt= */ {}, base::byte_span_from_cstring(kHkdfInfo),
                    base::span(derived_key));

  // WrapUnique is required since the AesGcmCrypter c'tor is private.
  return base::WrapUnique(new AesGcmCrypter(std::move(derived_key)));
}

std::optional<std::vector<uint8_t>> EcdhKeyExchange::AesGcmCrypter::Encrypt(
    base::span<const uint8_t> plaintext) {
  const auto nonce_bytes = crypto::RandBytesAsArray<kAesGcmIvSizeBytes>();
  std::vector<uint8_t> ciphertext =
      context_->Seal(plaintext, nonce_bytes, base::span<const uint8_t>());
  if (ciphertext.empty()) {
    return std::nullopt;
  }
  std::vector<uint8_t> result;
  result.reserve(nonce_bytes.size() + ciphertext.size());
  result.insert(result.end(), nonce_bytes.begin(), nonce_bytes.end());
  result.insert(result.end(), ciphertext.begin(), ciphertext.end());
  return result;
}

std::optional<std::vector<uint8_t>> EcdhKeyExchange::AesGcmCrypter::Decrypt(
    base::span<const uint8_t> ciphertext_iv) {
  if (ciphertext_iv.size() < kAesGcmIvSizeBytes) {
    return std::nullopt;
  }

  auto [nonce, ciphertext] =
      base::span(ciphertext_iv).split_at(kAesGcmIvSizeBytes);
  return context_->Open(ciphertext, nonce, /*additional_data=*/{});
}

}  // namespace remoting

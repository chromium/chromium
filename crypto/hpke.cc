// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hpke.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace crypto::hpke {

namespace {

bool ParamsAreSupported(const HpkeParams& params) {
  return params.kem == KemType::kX25519HkdfSha256 &&
         params.kdf == KdfType::kHkdfSha256 &&
         (params.aead == AeadType::kChaCha20Poly1305 ||
          params.aead == AeadType::kAes128Gcm);
}

const EVP_HPKE_AEAD* GetAead(AeadType aead) {
  switch (aead) {
    case AeadType::kChaCha20Poly1305:
      return EVP_hpke_chacha20_poly1305();
    case AeadType::kAes128Gcm:
      return EVP_hpke_aes_128_gcm();
  }
  NOTREACHED();
}

std::optional<std::vector<uint8_t>> SealInternal(
    EVP_HPKE_CTX* ctx,
    std::vector<uint8_t> encrypted_data,
    size_t encapsulated_shared_secret_len,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> ad) {
  encrypted_data.resize(encapsulated_shared_secret_len + plaintext.size() +
                        EVP_HPKE_CTX_max_overhead(ctx));

  base::span<uint8_t> ciphertext =
      base::span(encrypted_data).subspan(encapsulated_shared_secret_len);
  size_t ciphertext_len;

  if (!EVP_HPKE_CTX_seal(
          /*ctx=*/ctx, /*out=*/ciphertext.data(),
          /*out_len=*/&ciphertext_len,
          /*max_out_len=*/ciphertext.size(), /*in=*/plaintext.data(),
          /*in_len=*/plaintext.size(),
          /*ad=*/ad.data(),
          /*ad_len=*/ad.size())) {
    return std::nullopt;
  }

  encrypted_data.resize(encapsulated_shared_secret_len + ciphertext_len);
  return encrypted_data;
}

std::optional<std::vector<uint8_t>> OpenInternal(
    EVP_HPKE_CTX* ctx,
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> ad) {
  std::vector<uint8_t> plaintext(ciphertext.size());
  size_t plaintext_len;

  if (!EVP_HPKE_CTX_open(
          /*ctx=*/ctx, /*out=*/plaintext.data(),
          /*out_len=*/&plaintext_len, /*max_out_len=*/plaintext.size(),
          /*in=*/ciphertext.data(), /*in_len=*/ciphertext.size(),
          /*ad=*/ad.data(),
          /*ad_len=*/ad.size())) {
    return std::nullopt;
  }

  plaintext.resize(plaintext_len);
  return plaintext;
}

}  // namespace

std::optional<std::vector<uint8_t>> AuthSeal(
    const HpkeParams& params,
    const crypto::keypair::PrivateKey& sender,
    const crypto::keypair::PublicKey& recipient,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad) {
  CHECK(ParamsAreSupported(params));

  auto sender_priv_raw = sender.ToX25519PrivateKey();
  auto recipient_pub_raw = recipient.ToX25519PublicKey();

  bssl::ScopedEVP_HPKE_KEY sender_key;
  // EVP_HPKE_KEY_init returns 1 on success and 0 on failure.
  // Failure here would likely be due to invalid key or OOM, which we CHECK.
  CHECK(EVP_HPKE_KEY_init(sender_key.get(), EVP_hpke_x25519_hkdf_sha256(),
                          sender_priv_raw.data(), sender_priv_raw.size()));

  bssl::ScopedEVP_HPKE_CTX sender_ctx;
  std::vector<uint8_t> encrypted_data(EVP_HPKE_MAX_ENC_LENGTH);
  size_t encapsulated_shared_secret_len;

  const EVP_HPKE_AEAD* aead = GetAead(params.aead);

  if (!EVP_HPKE_CTX_setup_auth_sender(
          /*ctx=*/sender_ctx.get(),
          /*out_enc=*/encrypted_data.data(),
          /*out_enc_len=*/&encapsulated_shared_secret_len,
          /*max_enc=*/encrypted_data.size(), sender_key.get(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/aead,
          /*peer_public_key=*/recipient_pub_raw.data(),
          /*peer_public_key_len=*/recipient_pub_raw.size(),
          /*info=*/info.data(),
          /*info_len=*/info.size())) {
    return std::nullopt;
  }

  return SealInternal(sender_ctx.get(), std::move(encrypted_data),
                      encapsulated_shared_secret_len, plaintext, ad);
}

std::optional<std::vector<uint8_t>> AuthOpen(
    const HpkeParams& params,
    const crypto::keypair::PrivateKey& recipient,
    const crypto::keypair::PublicKey& sender,
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad) {
  CHECK(ParamsAreSupported(params));

  if (encrypted_data.size() < X25519_PUBLIC_VALUE_LEN) {
    return std::nullopt;
  }

  auto recipient_priv_raw = recipient.ToX25519PrivateKey();
  auto sender_pub_raw = sender.ToX25519PublicKey();

  bssl::ScopedEVP_HPKE_KEY recipient_key;
  CHECK(EVP_HPKE_KEY_init(recipient_key.get(), EVP_hpke_x25519_hkdf_sha256(),
                          recipient_priv_raw.data(),
                          recipient_priv_raw.size()));

  auto [enc, ciphertext] =
      encrypted_data.split_at(static_cast<size_t>(X25519_PUBLIC_VALUE_LEN));

  bssl::ScopedEVP_HPKE_CTX recipient_ctx;
  const EVP_HPKE_AEAD* aead = GetAead(params.aead);

  if (!EVP_HPKE_CTX_setup_auth_recipient(
          /*ctx=*/recipient_ctx.get(), /*key=*/recipient_key.get(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/aead,
          /*enc=*/enc.data(), /*enc_len=*/enc.size(),
          /*info=*/info.data(),
          /*info_len=*/info.size(),
          /*peer_public_key=*/sender_pub_raw.data(),
          /*peer_public_key_len=*/sender_pub_raw.size())) {
    return std::nullopt;
  }

  return OpenInternal(recipient_ctx.get(), ciphertext, ad);
}

std::optional<std::vector<uint8_t>> Seal(
    const HpkeParams& params,
    const crypto::keypair::PublicKey& recipient,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad) {
  CHECK(ParamsAreSupported(params));

  auto recipient_pub_raw = recipient.ToX25519PublicKey();

  bssl::ScopedEVP_HPKE_CTX sender_ctx;
  std::vector<uint8_t> encrypted_data(EVP_HPKE_MAX_ENC_LENGTH);
  size_t encapsulated_shared_secret_len;

  const EVP_HPKE_AEAD* aead = GetAead(params.aead);

  if (!EVP_HPKE_CTX_setup_sender(
          /*ctx=*/sender_ctx.get(),
          /*out_enc=*/encrypted_data.data(),
          /*out_enc_len=*/&encapsulated_shared_secret_len,
          /*max_enc=*/encrypted_data.size(),
          /*kem=*/EVP_hpke_x25519_hkdf_sha256(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/aead,
          /*peer_public_key=*/recipient_pub_raw.data(),
          /*peer_public_key_len=*/recipient_pub_raw.size(),
          /*info=*/info.data(),
          /*info_len=*/info.size())) {
    return std::nullopt;
  }

  return SealInternal(sender_ctx.get(), std::move(encrypted_data),
                      encapsulated_shared_secret_len, plaintext, ad);
}

std::optional<std::vector<uint8_t>> Open(
    const HpkeParams& params,
    const crypto::keypair::PrivateKey& receiver,
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad) {
  CHECK(ParamsAreSupported(params));

  if (encrypted_data.size() < X25519_PUBLIC_VALUE_LEN) {
    return std::nullopt;
  }

  auto receiver_priv_raw = receiver.ToX25519PrivateKey();

  bssl::ScopedEVP_HPKE_KEY receiver_key;
  CHECK(EVP_HPKE_KEY_init(receiver_key.get(), EVP_hpke_x25519_hkdf_sha256(),
                          receiver_priv_raw.data(), receiver_priv_raw.size()));

  auto [enc, ciphertext] =
      encrypted_data.split_at(size_t{X25519_PUBLIC_VALUE_LEN});

  bssl::ScopedEVP_HPKE_CTX recipient_ctx;
  const EVP_HPKE_AEAD* aead = GetAead(params.aead);

  if (!EVP_HPKE_CTX_setup_recipient(
          /*ctx=*/recipient_ctx.get(), /*key=*/receiver_key.get(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/aead,
          /*enc=*/enc.data(), /*enc_len=*/enc.size(),
          /*info=*/info.data(),
          /*info_len=*/info.size())) {
    return std::nullopt;
  }

  return OpenInternal(recipient_ctx.get(), ciphertext, ad);
}

}  // namespace crypto::hpke

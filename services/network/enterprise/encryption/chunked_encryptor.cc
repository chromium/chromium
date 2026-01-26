// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/chunked_encryptor.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/numerics/byte_conversions.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"

namespace network::enterprise_encryption {

EncryptionContext::EncryptionContext(
    base::span<const uint8_t, kKeySize> key,
    base::span<const uint8_t, kNoncePrefixSize> prefix)
    : derived_key(std::string(key.begin(), key.end())) {
  base::span(nonce_prefix).copy_from(prefix);
}

EncryptionContext::EncryptionContext(EncryptionContext&&) = default;
EncryptionContext& EncryptionContext::operator=(EncryptionContext&&) = default;
EncryptionContext::~EncryptionContext() = default;

base::expected<std::pair<std::vector<uint8_t>, EncryptionContext>,
               EncryptionError>
CreateHeader(base::span<const uint8_t> key_value) {
  if (key_value.empty() || key_value.size() != kKeySize) {
    return base::unexpected(EncryptionError::kInvalidKey);
  }

  // Populate header with header size, random salt and random nonce prefix.
  std::array<uint8_t, kSaltSize> salt;
  crypto::RandBytes(salt);

  std::array<uint8_t, kNoncePrefixSize> nonce_prefix;
  crypto::RandBytes(nonce_prefix);

  std::vector<uint8_t> header(kHeaderSize);
  base::SpanWriter writer(base::as_writable_byte_span(header));

  CHECK(writer.WriteU8BigEndian(kHeaderSize));
  CHECK(writer.Write(salt));
  CHECK(writer.Write(nonce_prefix));

  // Derive per-file key using key_value and salt.
  auto derived_key = crypto::HkdfSha256<kKeySize>(key_value, salt, {});

  // Create EncryptionContext. Used for encryption/decryption.
  EncryptionContext encryption_context(derived_key, nonce_prefix);

  return std::make_pair(std::move(header), std::move(encryption_context));
}

base::expected<EncryptionContext, EncryptionError> ParseHeader(
    base::span<const uint8_t> header,
    base::span<const uint8_t> key_value) {
  if (key_value.empty() || key_value.size() != kKeySize) {
    return base::unexpected(EncryptionError::kInvalidKey);
  }

  base::SpanReader reader(header);

  // Read in and check header size.
  uint8_t size_byte = 0;
  if (!reader.ReadU8BigEndian(size_byte) || size_byte != kHeaderSize) {
    return base::unexpected(EncryptionError::kInvalidHeader);
  }

  // Read salt and nonce prefix.
  auto salt = reader.Read<kSaltSize>();
  auto nonce_prefix = reader.Read<kNoncePrefixSize>();

  if (!salt || !nonce_prefix) {
    return base::unexpected(EncryptionError::kInvalidHeader);
  }

  // Create derived key for file-specific encryption/decryption.
  auto derived_key = crypto::HkdfSha256<kKeySize>(key_value, *salt, {});

  return EncryptionContext(derived_key, *nonce_prefix);
}

ChunkedEncryptor::ChunkedEncryptor(const EncryptionContext& encryption_context)
    : nonce_prefix_(encryption_context.nonce_prefix),
      aead_(crypto::Aead::AES_256_GCM_SIV) {
  aead_.Init(base::as_byte_span(encryption_context.derived_key.secure_value()));
}

ChunkedEncryptor::~ChunkedEncryptor() = default;

std::array<uint8_t, kNonceSize> ChunkedEncryptor::CreateNonce(
    uint32_t chunk_index,
    bool is_last_chunk) const {
  std::array<uint8_t, kNonceSize> nonce;
  base::SpanWriter<uint8_t> writer(nonce);
  CHECK(writer.Write(nonce_prefix_));
  CHECK(writer.WriteU32BigEndian(chunk_index));
  CHECK(writer.WriteU8BigEndian(is_last_chunk ? 0x01 : 0x00));

  return nonce;
}

std::vector<uint8_t> ChunkedEncryptor::EncryptChunk(
    base::span<const uint8_t> plaintext,
    uint32_t chunk_index,
    bool is_last_chunk) {
  // Nonce construction.
  std::array<uint8_t, kNonceSize> nonce =
      CreateNonce(chunk_index, is_last_chunk);

  std::vector<uint8_t> ciphertext =
      aead_.Seal(plaintext, nonce, base::span<const uint8_t>());

  CHECK_EQ(ciphertext.size(), plaintext.size() + kAuthTagSize);

  return ciphertext;
}

base::expected<std::vector<uint8_t>, EncryptionError>
ChunkedEncryptor::DecryptChunk(base::span<const uint8_t> ciphertext,
                               uint32_t chunk_index,
                               bool is_last_chunk) {
  if (ciphertext.size() < kAuthTagSize) {
    return base::unexpected(EncryptionError::kDecryptionFailed);
  }

  // Nonce construction.
  std::array<uint8_t, kNonceSize> nonce =
      CreateNonce(chunk_index, is_last_chunk);

  auto plaintext_opt =
      aead_.Open(ciphertext, nonce, base::span<const uint8_t>());

  if (!plaintext_opt) {
    return base::unexpected(EncryptionError::kDecryptionFailed);
  }

  return std::move(*plaintext_opt);
}

}  // namespace network::enterprise_encryption

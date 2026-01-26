// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/chunked_encryptor.h"

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "crypto/random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::enterprise_encryption {

namespace {

std::vector<uint8_t> CreateTestKey() {
  return std::vector<uint8_t>(kKeySize, 'A');
}

}  // namespace

class ChunkedEncryptorTest : public testing::Test {};

TEST_F(ChunkedEncryptorTest, CreateHeader) {
  // Empty key check.
  EXPECT_EQ(CreateHeader({}), base::unexpected(EncryptionError::kInvalidKey));

  std::vector<uint8_t> master_key = CreateTestKey();

  auto result = CreateHeader(master_key);
  ASSERT_TRUE(result.has_value());

  const auto& [header, encryption_context] = result.value();

  // Verify sizes.
  EXPECT_EQ(header.size(), kHeaderSize);
  EXPECT_EQ(header[0], kHeaderSize);
  EXPECT_EQ(encryption_context.nonce_prefix.size(), kNoncePrefixSize);
  EXPECT_EQ(encryption_context.derived_key.size(), kKeySize);
}

TEST_F(ChunkedEncryptorTest, ParseHeader_Success) {
  std::vector<uint8_t> master_key = CreateTestKey();
  auto header_result = CreateHeader(master_key);
  ASSERT_TRUE(header_result.has_value());

  auto parse_result = ParseHeader(header_result->first, master_key);
  ASSERT_TRUE(parse_result.has_value());

  const EncryptionContext& parsed_data = parse_result.value();
  const EncryptionContext& original_encryption_context = header_result->second;

  // Verify derived data matches.
  EXPECT_EQ(parsed_data.derived_key.secure_value(),
            original_encryption_context.derived_key.secure_value());
  EXPECT_EQ(parsed_data.nonce_prefix, original_encryption_context.nonce_prefix);
}

TEST_F(ChunkedEncryptorTest, ParseHeader_Failures) {
  std::vector<uint8_t> master_key = CreateTestKey();

  // Wrong size.
  std::vector<uint8_t> short_header(kHeaderSize - 1, 0);
  EXPECT_EQ(ParseHeader(short_header, master_key),
            base::unexpected(EncryptionError::kInvalidHeader));

  // Empty key.
  std::vector<uint8_t> valid_header(kHeaderSize, 0);
  valid_header[0] = kHeaderSize;
  EXPECT_EQ(ParseHeader(valid_header, {}),
            base::unexpected(EncryptionError::kInvalidKey));
}

TEST_F(ChunkedEncryptorTest, CreateNonce_Structure) {
  std::vector<uint8_t> master_key = CreateTestKey();
  auto result = CreateHeader(master_key);
  ASSERT_TRUE(result.has_value());
  const EncryptionContext& encryption_context = result->second;

  ChunkedEncryptor encryptor(encryption_context);

  uint32_t chunk_index = 0x12345678;
  bool is_last_chunk = true;

  std::array<uint8_t, kNonceSize> nonce =
      encryptor.CreateNonce(chunk_index, is_last_chunk);

  // Verify prefix.
  EXPECT_EQ(base::span(nonce).first<kNoncePrefixSize>(),
            base::span(encryption_context.nonce_prefix));

  // Verify index (Big Endian).
  auto index_span = base::span(nonce).subspan(kNoncePrefixSize, 4u);
  EXPECT_EQ(index_span[0], 0x12);
  EXPECT_EQ(index_span[1], 0x34);
  EXPECT_EQ(index_span[2], 0x56);
  EXPECT_EQ(index_span[3], 0x78);

  // Verify flag.
  EXPECT_EQ(nonce.back(), 0x01);

  // Verify flag for non-last chunk.
  nonce = encryptor.CreateNonce(chunk_index, false);
  EXPECT_EQ(nonce.back(), 0x00);
}

TEST_F(ChunkedEncryptorTest, EncryptDecrypt_RoundTrip) {
  std::vector<uint8_t> master_key = CreateTestKey();
  auto result = CreateHeader(master_key);
  ASSERT_TRUE(result.has_value());
  const EncryptionContext& encryption_context = result->second;

  ChunkedEncryptor encryptor(encryption_context);

  std::string plaintext_str = "Test string!";
  base::span<const uint8_t> plaintext = base::as_byte_span(plaintext_str);

  // Encrypt operation.
  std::vector<uint8_t> ciphertext = encryptor.EncryptChunk(plaintext, 1, false);

  // Decrypt operation.
  auto decrypted_result = encryptor.DecryptChunk(ciphertext, 1, false);
  ASSERT_TRUE(decrypted_result.has_value());
  std::vector<uint8_t> decrypted = std::move(decrypted_result.value());

  // Verify plaintext matches.
  EXPECT_EQ(plaintext.size(), decrypted.size());
  EXPECT_EQ(decrypted, base::as_bytes(base::span(plaintext)));
}

TEST_F(ChunkedEncryptorTest, Decrypt_WrongContext) {
  std::vector<uint8_t> master_key = CreateTestKey();
  auto result = CreateHeader(master_key);
  ASSERT_TRUE(result.has_value());
  const EncryptionContext& encryption_context = result->second;

  ChunkedEncryptor encryptor(encryption_context);
  base::span<const uint8_t> plaintext = base::as_byte_span("Secret");

  std::vector<uint8_t> ciphertext = encryptor.EncryptChunk(plaintext, 1, false);

  // Wrong index.
  EXPECT_EQ(encryptor.DecryptChunk(ciphertext, 2, false),
            base::unexpected(EncryptionError::kDecryptionFailed));

  // Wrong flag.
  EXPECT_EQ(encryptor.DecryptChunk(ciphertext, 1, true),
            base::unexpected(EncryptionError::kDecryptionFailed));

  // Tampered ciphertext.
  ciphertext[0] ^= 0xFF;
  EXPECT_EQ(encryptor.DecryptChunk(ciphertext, 1, false),
            base::unexpected(EncryptionError::kDecryptionFailed));
}

TEST_F(ChunkedEncryptorTest, EmptyPlaintext) {
  std::vector<uint8_t> master_key = CreateTestKey();
  auto result = CreateHeader(master_key);
  ASSERT_TRUE(result.has_value());
  const EncryptionContext& encryption_context = result->second;

  ChunkedEncryptor encryptor(encryption_context);

  // Encrypt empty.
  std::vector<uint8_t> ciphertext_result = encryptor.EncryptChunk({}, 0, true);
  EXPECT_EQ(ciphertext_result.size(), kAuthTagSize);

  // Decrypt empty.
  auto decrypted_result = encryptor.DecryptChunk(ciphertext_result, 0, true);
  ASSERT_TRUE(decrypted_result.has_value());
  EXPECT_TRUE(decrypted_result->empty());
}

}  // namespace network::enterprise_encryption

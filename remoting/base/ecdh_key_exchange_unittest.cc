// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/ecdh_key_exchange.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

std::pair<std::unique_ptr<EcdhKeyExchange::AesGcmCrypter>,
          std::unique_ptr<EcdhKeyExchange::AesGcmCrypter>>
MakeCrypterPair() {
  EcdhKeyExchange key_exchange_1;
  EcdhKeyExchange key_exchange_2;
  return std::pair(
      key_exchange_1.CreateAesGcmCrypter(key_exchange_2.public_key_bytes()),
      key_exchange_2.CreateAesGcmCrypter(key_exchange_1.public_key_bytes()));
}

}  // namespace

TEST(EcdhKeyExchangeTest, EncryptAndDecrypt) {
  auto [host_crypter, client_crypter] = MakeCrypterPair();
  std::string message = "Hello, World!";
  std::optional<std::vector<uint8_t>> ciphertext =
      host_crypter->Encrypt(base::as_bytes(base::span(message)));
  ASSERT_TRUE(ciphertext.has_value());

  std::optional<std::vector<uint8_t>> plaintext =
      client_crypter->Decrypt(*ciphertext);
  ASSERT_TRUE(plaintext.has_value());

  ASSERT_EQ(std::string(plaintext->begin(), plaintext->end()), message);
}

TEST(EcdhKeyExchangeTest, DecryptFailsWithWrongCrypter) {
  EcdhKeyExchange key_exchange_1;
  EcdhKeyExchange key_exchange_2;
  EcdhKeyExchange key_exchange_3;

  auto crypter1 =
      key_exchange_1.CreateAesGcmCrypter(key_exchange_2.public_key_bytes());
  ASSERT_TRUE(crypter1);
  auto crypter3 =
      key_exchange_3.CreateAesGcmCrypter(key_exchange_2.public_key_bytes());
  ASSERT_TRUE(crypter3);

  std::string message = "Hello, World!";
  std::optional<std::vector<uint8_t>> ciphertext =
      crypter1->Encrypt(base::as_bytes(base::span(message)));
  ASSERT_TRUE(ciphertext.has_value());

  // Decryption should fail as crypter3 used a different key-pair.
  ASSERT_FALSE(crypter3->Decrypt(*ciphertext).has_value());
}

TEST(EcdhKeyExchangeTest, CreateAesGcmCrypterFailsWithInvalidPeerKey) {
  EcdhKeyExchange key_exchange;
  std::vector<uint8_t> invalid_key(32, 0);
  ASSERT_FALSE(key_exchange.CreateAesGcmCrypter(invalid_key));
}

TEST(EcdhKeyExchangeTest, DecryptFailsWithTruncatedCiphertext) {
  auto [_, client_crypter] = MakeCrypterPair();

  // IV is 12 bytes, so anything less than that is invalid.
  std::vector<uint8_t> truncated_ciphertext(11, 0);
  ASSERT_FALSE(client_crypter->Decrypt(truncated_ciphertext).has_value());
}

TEST(EcdhKeyExchangeTest, DecryptFailsWithTamperedCiphertext) {
  auto [host_crypter, client_crypter] = MakeCrypterPair();

  std::string message = "Hello, World!";
  std::optional<std::vector<uint8_t>> ciphertext =
      host_crypter->Encrypt(base::as_bytes(base::span(message)));
  ASSERT_TRUE(ciphertext.has_value());

  // Tamper with the ciphertext (flip a bit).
  (*ciphertext)[ciphertext->size() - 1] ^= 1;

  ASSERT_FALSE(client_crypter->Decrypt(*ciphertext).has_value());
}

TEST(EcdhKeyExchangeTest, EncryptAndDecryptEmptyMessage) {
  auto [host_crypter, client_crypter] = MakeCrypterPair();

  std::string message = "";
  std::optional<std::vector<uint8_t>> ciphertext =
      host_crypter->Encrypt(base::as_bytes(base::span(message)));
  ASSERT_TRUE(ciphertext.has_value());

  std::optional<std::vector<uint8_t>> plaintext =
      client_crypter->Decrypt(*ciphertext);
  ASSERT_TRUE(plaintext.has_value());
  ASSERT_EQ(plaintext->size(), 0u);
  ASSERT_EQ(std::string(plaintext->begin(), plaintext->end()), message);
}

TEST(EcdhKeyExchangeTest, PublicKeyBase64ReturnsValidBase64) {
  EcdhKeyExchange key_exchange;
  std::string public_key = key_exchange.PublicKeyBase64();
  ASSERT_FALSE(public_key.empty());
  std::optional<std::vector<uint8_t>> decoded_key =
      base::Base64Decode(public_key);
  ASSERT_TRUE(decoded_key.has_value());
  ASSERT_EQ(key_exchange.public_key_bytes(), decoded_key.value());
}

}  // namespace remoting

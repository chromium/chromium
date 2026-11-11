// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_payload_parser.h"

#import <string>
#import <vector>

#import "base/base64.h"
#import "base/base64url.h"
#import "base/containers/span.h"
#import "crypto/aead.h"
#import "crypto/random.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_keychain_util.h"
#import "ios/chrome/browser/passwords/model/remote_actor/test/remote_actor_test_util.h"
#import "ios/chrome/browser/passwords/model/remote_actor/test/scoped_keychain_access_group_override.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using remote_actor::proto::RemoteActorPasswordUploadRequest;

constexpr char kTestGaiaId[] = "123456789";
constexpr char kTestDomain[] = "https://example.com";
constexpr char kTestTaskId[] = "task_abc_123";
constexpr size_t kAes256KeySize = 32;

std::vector<uint8_t> CreateRandomKey() {
  return crypto::RandBytesAsVector(kAes256KeySize);
}

RemoteActorPasswordUploadRequest CreateValidRequest() {
  RemoteActorPasswordUploadRequest request;
  request.set_gaia_id(kTestGaiaId);
  request.set_domain(kTestDomain);
  request.set_task_id(kTestTaskId);
  return request;
}

using RemoteActorPayloadParserTest = PlatformTest;

// Tests that a valid request is successfully encrypted, serialized, and
// decrypted.
TEST_F(RemoteActorPayloadParserTest, RoundtripEncryptionAndDecryption) {
  std::vector<uint8_t> key = CreateRandomKey();
  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();

  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(encrypted_payload->first,
                                       encrypted_payload->second, key);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->gaia_id(), kTestGaiaId);
  EXPECT_EQ(result->domain(), kTestDomain);
  EXPECT_EQ(result->task_id(), kTestTaskId);
}

// Tests that URL-safe Base64 encoded payload and nonce are supported.
TEST_F(RemoteActorPayloadParserTest, UrlSafeBase64Decoding) {
  std::vector<uint8_t> key = CreateRandomKey();
  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();

  std::string proto_bytes;
  ASSERT_TRUE(original_request.SerializeToString(&proto_bytes));

  std::vector<uint8_t> nonce = crypto::RandBytesAsVector(
      crypto::aead::NonceSizeFor(crypto::aead::AES_256_GCM));
  crypto::Aead aead(crypto::Aead::AES_256_GCM, key);
  std::vector<uint8_t> ciphertext =
      aead.Seal(base::as_byte_span(proto_bytes), nonce, /*additional_data=*/{});

  std::string url_payload;
  base::Base64UrlEncode(ciphertext, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &url_payload);

  std::string url_nonce;
  base::Base64UrlEncode(nonce, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &url_nonce);

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(url_payload, url_nonce, key);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->gaia_id(), kTestGaiaId);
  EXPECT_EQ(result->domain(), kTestDomain);
  EXPECT_EQ(result->task_id(), kTestTaskId);
}

// Tests that payloads exceeding the size limit are rejected.
TEST_F(RemoteActorPayloadParserTest, PayloadTooLarge) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::string large_payload(kMaxRemoteActorPayloadSizeBytes + 1, 'A');

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(large_payload, "valid_nonce", key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kPayloadTooLarge);
}

// Tests that nonces exceeding the size limit are rejected.
TEST_F(RemoteActorPayloadParserTest, NonceTooLarge) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::string large_nonce(kMaxRemoteActorNonceSizeBytes + 1, 'B');

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload("valid_payload", large_nonce, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kNonceTooLarge);
}

// Tests that malformed Base64 payloads are rejected.
TEST_F(RemoteActorPayloadParserTest, InvalidBase64Payload) {
  std::vector<uint8_t> key = CreateRandomKey();

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload("!@#$not_base64!", "valid_nonce", key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kInvalidBase64Payload);
}

// Tests that malformed Base64 nonces are rejected.
TEST_F(RemoteActorPayloadParserTest, InvalidBase64Nonce) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::string valid_base64_payload = base::Base64Encode("valid_payload_bytes");

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result =
          ParseRemoteActorPayload(valid_base64_payload, "!@#$not_base64!", key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kInvalidBase64Nonce);
}

// Tests that keys with invalid length are rejected.
TEST_F(RemoteActorPayloadParserTest, InvalidKeyLength) {
  std::vector<uint8_t> short_key = crypto::RandBytesAsVector(16);
  std::string valid_base64_payload = base::Base64Encode("payload");
  std::string valid_base64_nonce = base::Base64Encode("nonce_12_bytes");

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(valid_base64_payload, valid_base64_nonce,
                                       short_key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kInvalidKeyLength);
}

// Tests that tampered ciphertext causes AEAD authentication failure.
TEST_F(RemoteActorPayloadParserTest, TamperedCiphertext) {
  std::vector<uint8_t> key = CreateRandomKey();
  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();

  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  std::string raw_ciphertext;
  ASSERT_TRUE(base::Base64Decode(encrypted_payload->first, &raw_ciphertext));
  raw_ciphertext[0] ^= 0xFF;  // Tamper with first byte.

  std::string tampered_payload = base::Base64Encode(raw_ciphertext);

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(tampered_payload,
                                       encrypted_payload->second, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kDecryptionFailed);
}

// Tests that decrypting with the wrong key fails authentication.
TEST_F(RemoteActorPayloadParserTest, WrongKey) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::vector<uint8_t> wrong_key = CreateRandomKey();
  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();

  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(encrypted_payload->first,
                                       encrypted_payload->second, wrong_key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kDecryptionFailed);
}

// Tests that decrypting with the wrong nonce fails authentication.
TEST_F(RemoteActorPayloadParserTest, WrongNonce) {
  std::vector<uint8_t> key = CreateRandomKey();
  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();

  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  std::vector<uint8_t> wrong_nonce_bytes = crypto::RandBytesAsVector(12);
  std::string wrong_nonce = base::Base64Encode(wrong_nonce_bytes);

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result =
          ParseRemoteActorPayload(encrypted_payload->first, wrong_nonce, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kDecryptionFailed);
}

// Tests that successfully decrypted data that is not a valid proto is rejected.
TEST_F(RemoteActorPayloadParserTest, InvalidProtobuf) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::vector<uint8_t> nonce = crypto::RandBytesAsVector(12);

  std::string garbage_bytes = "not_a_valid_protobuf_message";
  crypto::Aead aead(crypto::Aead::AES_256_GCM, key);
  std::vector<uint8_t> ciphertext = aead.Seal(base::as_byte_span(garbage_bytes),
                                              nonce, /*additional_data=*/{});

  std::string base64_payload = base::Base64Encode(ciphertext);
  std::string base64_nonce = base::Base64Encode(nonce);

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(base64_payload, base64_nonce, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kInvalidProtobuf);
}

// Tests that requests with missing required fields are rejected.
TEST_F(RemoteActorPayloadParserTest, MissingRequiredField) {
  std::vector<uint8_t> key = CreateRandomKey();

  RemoteActorPasswordUploadRequest request_without_domain;
  request_without_domain.set_gaia_id(kTestGaiaId);
  request_without_domain.set_task_id(kTestTaskId);
  // domain intentionally omitted.

  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(request_without_domain, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(encrypted_payload->first,
                                       encrypted_payload->second, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kMissingRequiredField);
}

// Tests that parsing fails with kMissingKeyInKeychain when keychain lookup
// returns nullopt.
TEST_F(RemoteActorPayloadParserTest, KeychainRetrievalMissingKey) {
  ScopedRemoteActorKeychainAccessGroupOverrideForTesting access_group_override(
      nil);
  DeleteRemoteActorSharedSymmetricKeyForTesting(
      nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService);

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload("payload", "nonce");

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorParsingError::kMissingKeyInKeychain);
}

// Tests that the Keychain symmetric key is deleted upon successful decryption
// to prevent replay attacks.
TEST_F(RemoteActorPayloadParserTest, DeletesKeychainKeyOnSuccessfulDecryption) {
  ScopedRemoteActorKeychainAccessGroupOverrideForTesting access_group_override(
      nil);
  std::vector<uint8_t> key = CreateRandomKey();
  ASSERT_TRUE(SetRemoteActorSharedSymmetricKeyForTesting(
      key, nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService));

  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();
  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      result = ParseRemoteActorPayload(encrypted_payload->first,
                                       encrypted_payload->second);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->gaia_id(), kTestGaiaId);
  EXPECT_EQ(result->domain(), kTestDomain);
  EXPECT_EQ(result->task_id(), kTestTaskId);

  // Key should be deleted after successful decryption, blocking replays.
  EXPECT_FALSE(GetRemoteActorSharedSymmetricKey().has_value());
  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      replay_result = ParseRemoteActorPayload(encrypted_payload->first,
                                              encrypted_payload->second);
  ASSERT_FALSE(replay_result.has_value());
  EXPECT_EQ(replay_result.error(),
            RemoteActorParsingError::kMissingKeyInKeychain);
}

// Tests that the Keychain symmetric key is preserved when decryption fails so
// a forged URL cannot discard a pending handoff key.
TEST_F(RemoteActorPayloadParserTest, PreservesKeychainKeyOnFailedDecryption) {
  ScopedRemoteActorKeychainAccessGroupOverrideForTesting access_group_override(
      nil);
  std::vector<uint8_t> key = CreateRandomKey();
  ASSERT_TRUE(SetRemoteActorSharedSymmetricKeyForTesting(
      key, nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService));

  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();
  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  std::string raw_ciphertext;
  ASSERT_TRUE(base::Base64Decode(encrypted_payload->first, &raw_ciphertext));
  // Flip all bits in the first byte to corrupt the ciphertext and trigger an
  // AEAD authentication/decryption failure.
  raw_ciphertext[0] ^= 0xFF;
  std::string tampered_payload = base::Base64Encode(raw_ciphertext);

  base::expected<RemoteActorPasswordUploadRequest, RemoteActorParsingError>
      failed_result =
          ParseRemoteActorPayload(tampered_payload, encrypted_payload->second);
  ASSERT_FALSE(failed_result.has_value());
  EXPECT_EQ(failed_result.error(), RemoteActorParsingError::kDecryptionFailed);

  // Key should still be present in Keychain.
  EXPECT_EQ(GetRemoteActorSharedSymmetricKey(), key);

  DeleteRemoteActorSharedSymmetricKeyForTesting(
      nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService);
}

}  // namespace

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_payload_parser.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import <string>
#import <string_view>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/base64url.h"
#import "base/containers/span.h"
#import "base/containers/to_vector.h"
#import "crypto/aead.h"
#import "crypto/random.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace remote_actor {
namespace {

using proto::RemoteActorPasswordUploadRequest;

constexpr char kTestGaiaId[] = "123456789";
constexpr char kTestDomain[] = "https://example.com";
constexpr char kTestTaskId[] = "task_abc_123";
constexpr size_t kAes256KeySize = 32;

NSString* const kTestKeychainAccount = @"TestRemoteActorAccount";
NSString* const kTestKeychainService = @"TestRemoteActorService";

// Retrieves the symmetric key using custom Keychain attributes `access_group`,
// `account`, and `service`. Returns `std::nullopt` if the key does not exist or
// an error occurs.
std::optional<std::vector<uint8_t>> GetSharedSymmetricKey(
    NSString* access_group,
    NSString* account,
    NSString* service) {
  NSMutableDictionary* query = [@{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : account,
    (__bridge id)kSecAttrService : service,
    (__bridge id)kSecReturnData : @YES,
    (__bridge id)kSecMatchLimit : (__bridge id)kSecMatchLimitOne,
  } mutableCopy];

  if (access_group.length > 0) {
    query[(__bridge id)kSecAttrAccessGroup] = access_group;
  }

  CFTypeRef result = nullptr;
  OSStatus status =
      SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
  if (status != errSecSuccess || !result) {
    return std::nullopt;
  }

  NSData* data = (__bridge_transfer NSData*)result;
  return base::ToVector(base::apple::NSDataToSpan(data));
}

// Deletes the symmetric key identified by `access_group`, `account`, and
// `service` from the Keychain. Returns `true` if deleted successfully or if the
// key did not exist.
bool DeleteSharedSymmetricKey(NSString* access_group,
                              NSString* account,
                              NSString* service) {
  NSMutableDictionary* query = [@{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : account,
    (__bridge id)kSecAttrService : service,
  } mutableCopy];

  if (access_group.length > 0) {
    query[(__bridge id)kSecAttrAccessGroup] = access_group;
  }

  OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
  return status == errSecSuccess || status == errSecItemNotFound;
}

// Stores `key` in the Keychain under `access_group`, `account`, and `service`
// for testing or mock purposes. Returns `true` if stored successfully.
bool SetSharedSymmetricKey(base::span<const uint8_t> key,
                           NSString* access_group,
                           NSString* account,
                           NSString* service) {
  DeleteSharedSymmetricKey(access_group, account, service);

  NSMutableDictionary* attributes = [@{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : account,
    (__bridge id)kSecAttrService : service,
    (__bridge id)kSecValueData : [NSData dataWithBytes:key.data()
                                                length:key.size()],
  } mutableCopy];

  if (access_group.length > 0) {
    attributes[(__bridge id)kSecAttrAccessGroup] = access_group;
  }

  OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attributes, nullptr);
  return status == errSecSuccess;
}

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

class RemoteActorPayloadParserTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    DeleteTestKeychainKeys();
  }

  void TearDown() override {
    DeleteTestKeychainKeys();
    PlatformTest::TearDown();
  }

 private:
  void DeleteTestKeychainKeys() {
    DeleteSharedSymmetricKey(/*access_group=*/nil, kTestKeychainAccount,
                             kTestKeychainService);
  }
};

// Tests that a valid request is successfully encrypted, serialized, and
// decrypted.
TEST_F(RemoteActorPayloadParserTest, RoundtripEncryptionAndDecryption) {
  std::vector<uint8_t> key = CreateRandomKey();
  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();

  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(encrypted_payload->first,
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

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(url_payload, url_nonce, key);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->gaia_id(), kTestGaiaId);
  EXPECT_EQ(result->domain(), kTestDomain);
  EXPECT_EQ(result->task_id(), kTestTaskId);
}

// Tests that payloads exceeding the size limit are rejected.
TEST_F(RemoteActorPayloadParserTest, PayloadTooLarge) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::string large_payload(kMaxPayloadSizeBytes + 1, 'A');

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(large_payload, "valid_nonce", key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kPayloadTooLarge);
}

// Tests that nonces exceeding the size limit are rejected.
TEST_F(RemoteActorPayloadParserTest, NonceTooLarge) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::string large_nonce(kMaxNonceSizeBytes + 1, 'B');

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload("valid_payload", large_nonce, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kNonceTooLarge);
}

// Tests that malformed Base64 payloads are rejected.
TEST_F(RemoteActorPayloadParserTest, InvalidBase64Payload) {
  std::vector<uint8_t> key = CreateRandomKey();

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload("!@#$not_base64!", "valid_nonce", key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kInvalidBase64Payload);
}

// Tests that malformed Base64 nonces are rejected.
TEST_F(RemoteActorPayloadParserTest, InvalidBase64Nonce) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::string valid_base64_payload = base::Base64Encode("valid_payload_bytes");

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(valid_base64_payload, "!@#$not_base64!", key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kInvalidBase64Nonce);
}

// Tests that keys with invalid length are rejected.
TEST_F(RemoteActorPayloadParserTest, InvalidKeyLength) {
  std::vector<uint8_t> short_key = crypto::RandBytesAsVector(16);
  std::string valid_base64_payload = base::Base64Encode("payload");
  std::string valid_base64_nonce = base::Base64Encode("nonce_12_bytes");

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(valid_base64_payload, valid_base64_nonce,
                              short_key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kInvalidKeyLength);
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

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(tampered_payload, encrypted_payload->second, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kDecryptionFailed);
}

// Tests that decrypting with the wrong key fails authentication.
TEST_F(RemoteActorPayloadParserTest, WrongKey) {
  std::vector<uint8_t> key = CreateRandomKey();
  std::vector<uint8_t> wrong_key = CreateRandomKey();
  RemoteActorPasswordUploadRequest original_request = CreateValidRequest();

  std::optional<std::pair<std::string, std::string>> encrypted_payload =
      CreateEncryptedRemoteActorPayloadForTesting(original_request, key);
  ASSERT_TRUE(encrypted_payload.has_value());

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(encrypted_payload->first,
                              encrypted_payload->second, wrong_key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kDecryptionFailed);
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

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(encrypted_payload->first, wrong_nonce, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kDecryptionFailed);
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

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(base64_payload, base64_nonce, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kInvalidProtobuf);
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

  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload(encrypted_payload->first,
                              encrypted_payload->second, key);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kMissingRequiredField);
}

// Tests that parsing fails with kMissingKeyInKeychain when keychain lookup
// returns nullopt.
TEST_F(RemoteActorPayloadParserTest, KeychainRetrievalMissingKey) {
  base::expected<RemoteActorPasswordUploadRequest, ParsingError> result =
      ParseRemoteActorPayload("payload", "nonce");

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParsingError::kMissingKeyInKeychain);
}

// Tests storing, retrieving, and deleting a symmetric key in Keychain without
// access group.
TEST_F(RemoteActorPayloadParserTest, KeychainStorageAndRetrieval) {
  std::vector<uint8_t> key = CreateRandomKey();

  EXPECT_TRUE(SetSharedSymmetricKey(key, nil, kTestKeychainAccount,
                                    kTestKeychainService));

  std::optional<std::vector<uint8_t>> retrieved_key =
      GetSharedSymmetricKey(nil, kTestKeychainAccount, kTestKeychainService);
  ASSERT_TRUE(retrieved_key.has_value());
  EXPECT_EQ(*retrieved_key, key);

  EXPECT_TRUE(DeleteSharedSymmetricKey(nil, kTestKeychainAccount,
                                       kTestKeychainService));

  std::optional<std::vector<uint8_t>> deleted_key =
      GetSharedSymmetricKey(nil, kTestKeychainAccount, kTestKeychainService);
  EXPECT_FALSE(deleted_key.has_value());
}

}  // namespace
}  // namespace remote_actor

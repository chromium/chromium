// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_model_utils.h"

#include <array>
#include <cstdint>

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/strings/string_view_util.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn::passkey_model_utils {
namespace {

using ::base::test::EqualsProto;
using ::base::test::ScopedFeatureList;
using ::webauthn::features::kDeviceAuthorizationPasskeyDecryption;

constexpr std::array<uint8_t, 32> kTestKey = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr int32_t kTestKeyVersion = 23;
constexpr std::string_view kRpId = "example.com";
static const PasskeyModel::UserEntity kTestUser(std::vector<uint8_t>{1, 2, 3},
                                                "user@example.com",
                                                "Example User");

sync_pb::WebauthnCredentialSpecifics CreateValidPasskey() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_rp_id("example.com");
  passkey.set_sync_id(base::RandBytesAsString(kSyncIdLength));
  passkey.set_credential_id(base::RandBytesAsString(kCredentialIdMinLength));
  passkey.set_user_id(base::RandBytesAsString(kUserIdMaxLength));
  passkey.set_private_key({1, 2, 3, 4});
  passkey.set_user_name("username");
  passkey.set_user_display_name("display_name");
  return passkey;
}

constexpr std::array<uint8_t, 32> kTestDeviceAuthorizationKey = {
    31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
    15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};

// A key that is neither `kTestKey` nor `kTestDeviceAuthorizationKey`.
constexpr std::array<uint8_t, 32> kWrongKey = {};

// A `security_domain_encrypted` value with the `CreateTestSecrets()` secrets,
// encrypted with `kTestKey` and `kTestDeviceAuthorizationKey`.
constexpr auto kTestSecurityDomainEncrypted = base::span_from_cstring(
    "\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x9d\xc7\xde\xd0"
    "\x51\x65\x77\x76\x24\x8e\x64\x4d\x12\x10\xd5\x74\x30\x73\x44\xa1"
    "\x77\xba\x83\x06\xec\x19\x43\x57\x60\x06\xf4\xa1\xb0\xb3\x57\x08"
    "\x98\x8a\x64\x29\xd6\x00\x87\x0a\x8f\x93\xd5\x5b\x64\x97\xca\x0a"
    "\x57\x87\xd2\xcd\x56\x71\x4e\xe6\xb1\x19\xe1\xb7\xf0\x48\xdc\x72"
    "\xc2\x94\x7c\x02\xb1\xb5\x12\x97\xea\xd4");

// Returns the secrets that `kTestSecurityDomainEncrypted` encrypts.
sync_pb::WebauthnCredentialSpecifics_Encrypted CreateTestSecrets() {
  sync_pb::WebauthnCredentialSpecifics_Encrypted secrets;
  secrets.set_private_key("testprivatekey");
  secrets.set_hmac_secret("testhmacsecret");
  return secrets;
}

// Returns a passkey whose `security_domain_encrypted` field is `encrypted`.
sync_pb::WebauthnCredentialSpecifics CreatePasskeyWithSecurityDomainEncrypted(
    base::span<const char> encrypted) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_security_domain_encrypted(base::as_string_view(encrypted));
  return passkey;
}

// Test decryption of the `encrypted` case for
// `WebAuthnCredentialSpecifics.encrypted_data`.
TEST(PasskeyModelUtilsTest, DecryptWebauthnCredentialSpecificsData_Encrypted) {
  static const struct {
    base::span<const char> encrypted;
    bool result;
    struct {
      std::string private_key;
      std::string hmac_secret;
      std::string cred_blob;
      std::string large_blob;
      uint64_t large_blob_uncompressed_size;
    } expected;
  } kTestCases[] = {
      {"", false, {}},
      // Short ciphertext
      {"\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a", false, {}},
      // Invalid message
      {"\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x24\xd4\xf2\x4f\x65"
       "\xab\x39\x94\x89\x6c\x9d\x27\x83\x0e\xac\x1a\xff",
       false,
       {}},
      // Empty message
      {"\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x9d\xfb\xc6\xda\x41"
       "\x6f\x5f\x7c\x06\x84\x02\xf8\x7f\x13\x61\x2f\xe4\xae\x37\x1b\x40\x7b"
       "\x8a\x65\x94\x65",
       true,
       {}},
      {"\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x9d\xf5\xa0\xbf\x28"
       "\x1b\x0d\x0e\x47\xf2\x0f\x33\x7d\x71\xe7\x62\x00\x25\xe4\xde\x99\x78"
       "\x0c\x2a\xa8\xe3\x30\x4c\xc2\x1e\xa4\x53\x25\xba\xdc\xa1\x21\xfb\x11"
       "\x0c\x40\x92\x08\xa8\x8f\xb8\x9f\xaa\xad\x51\xfc\xf9\x75\x9a\xbe\x91"
       "\x0e\xaf\xb4\x5c\x46\x0a\x05\x9e\xb2\xda\x98\xd0\xb3\x87\xd2\x3c\x52"
       "\x57\xb2\x57\x08\xb7\x18",
       true,
       {"testprivatekey", "testhmacsecret", "testcredblob", "testlargeblob",
        23}},
  };
  int i = 0;
  for (const auto& t : kTestCases) {
    SCOPED_TRACE(testing::Message() << i++);
    sync_pb::WebauthnCredentialSpecifics in;
    in.set_encrypted({t.encrypted.begin(), t.encrypted.end() - 1});
    sync_pb::WebauthnCredentialSpecifics_Encrypted out;
    EXPECT_EQ(DecryptWebauthnCredentialSpecificsData(
                  kTestKey, /*device_authorization_key=*/{}, in, &out),
              t.result);
    EXPECT_EQ(out.private_key(), t.expected.private_key);
    EXPECT_EQ(out.hmac_secret(), t.expected.hmac_secret);
    EXPECT_EQ(out.cred_blob(), t.expected.cred_blob);
    EXPECT_EQ(out.large_blob(), t.expected.large_blob);
    EXPECT_EQ(out.large_blob_uncompressed_size(),
              t.expected.large_blob_uncompressed_size);
  }
}

// Test decryption of the `private_key` case for
// `WebAuthnCredentialSpecifics.encrypted_data`.
TEST(PasskeyModelUtilsTest, DecryptWebauthnCredentialSpecificsData_PrivateKey) {
  static const struct {
    base::span<const char> encrypted;
    bool result;
    std::string expected_private_key;
  } kTestCases[] = {
      {"", false, {}},
      // Short ciphertext
      {"\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a", false, ""},
      // Empty key
      {"\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x54\x2a\x4c\x37\xe0"
       "\x35\xbc\xc6\x64\x9d\x57\x9c\x8f\x12\xe6\xa3",
       true, ""},
      {"\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\xe3\x9e\xa7\xae\x2b"
       "\x1d\x14\x0a\x4f\xf0\x0b\x2c\x7d\x63\xc6\x88\x33\x45\x2f\xbb\x29\x73"
       "\xff\xdd\xc2\x63\xfc\x57\xae\x3a",
       true, "testprivatekey"},
  };
  int i = 0;
  for (const auto& t : kTestCases) {
    SCOPED_TRACE(testing::Message() << i++);
    sync_pb::WebauthnCredentialSpecifics in;
    in.set_private_key({t.encrypted.begin(), t.encrypted.end() - 1});
    sync_pb::WebauthnCredentialSpecifics_Encrypted out;
    EXPECT_EQ(DecryptWebauthnCredentialSpecificsData(
                  kTestKey, /*device_authorization_key=*/{}, in, &out),
              t.result);
    EXPECT_EQ(out.private_key(), t.expected_private_key);
    EXPECT_FALSE(out.has_hmac_secret());
    EXPECT_FALSE(out.has_cred_blob());
    EXPECT_FALSE(out.has_large_blob());
    EXPECT_FALSE(out.has_large_blob_uncompressed_size());
  }
}

TEST(PasskeyModelUtilsTest, DecryptWebauthnCredentialSpecificsData_NotSet) {
  sync_pb::WebauthnCredentialSpecifics in;
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, /*device_authorization_key=*/{}, in, &out));
}

TEST(PasskeyModelUtilsTest, EncryptWebauthnCredentialSpecificsData) {
  sync_pb::WebauthnCredentialSpecifics_Encrypted plain;
  plain.set_private_key("a");
  plain.set_hmac_secret("b");
  plain.set_cred_blob("c");
  plain.set_large_blob("d");
  plain.set_large_blob_uncompressed_size(1u);
  sync_pb::WebauthnCredentialSpecifics encrypted;
  ASSERT_TRUE(
      EncryptWebauthnCredentialSpecificsData(kTestKey, plain, &encrypted));
  EXPECT_TRUE(encrypted.has_encrypted());

  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
  EXPECT_TRUE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, /*device_authorization_key=*/{}, encrypted, &decrypted));
  EXPECT_EQ(decrypted.private_key(), plain.private_key());
  EXPECT_EQ(decrypted.hmac_secret(), plain.hmac_secret());
  EXPECT_EQ(decrypted.cred_blob(), plain.cred_blob());
  EXPECT_EQ(decrypted.large_blob(), plain.large_blob());
  EXPECT_EQ(decrypted.large_blob_uncompressed_size(),
            plain.large_blob_uncompressed_size());
}

TEST(PasskeyModelUtilsTest, GeneratePasskeyAndEncryptSecrets) {
  auto [passkey, public_key_spki_der] = GeneratePasskeyAndEncryptSecrets(
      kRpId, kTestUser, kTestKey, kTestKeyVersion, /*extension_input_data=*/{},
      /*extension_output_data=*/nullptr);
  EXPECT_EQ(passkey.sync_id().size(), 16u);
  EXPECT_EQ(passkey.credential_id().size(), 16u);
  EXPECT_EQ(passkey.rp_id(), kRpId);
  EXPECT_EQ(passkey.user_id(),
            std::string(reinterpret_cast<const char*>(kTestUser.id.data()),
                        kTestUser.id.size()));
  EXPECT_EQ(passkey.user_name(), kTestUser.name);
  EXPECT_EQ(passkey.user_display_name(), kTestUser.display_name);
  EXPECT_FALSE(passkey.third_party_payments_support());
  EXPECT_EQ(passkey.last_used_time_windows_epoch_micros(), 0u);
  EXPECT_GT(passkey.creation_time(), 0u);
  EXPECT_EQ(passkey.key_version(), kTestKeyVersion);

  // Filled in by the Sync model.
  EXPECT_TRUE(passkey.newly_shadowed_credential_ids().empty());

  EXPECT_TRUE(passkey.has_encrypted());
  sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted_data;
  ASSERT_TRUE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, /*device_authorization_key=*/{}, passkey, &encrypted_data));
  EXPECT_FALSE(encrypted_data.private_key().empty());
  auto ec_key = crypto::keypair::PrivateKey::FromPrivateKeyInfo(
      base::as_byte_span(encrypted_data.private_key()));
  ASSERT_TRUE(ec_key.has_value());
  ASSERT_TRUE(ec_key->IsEcP256());
  std::vector<uint8_t> ec_key_pub = ec_key->ToSubjectPublicKeyInfo();
  EXPECT_EQ(ec_key_pub, public_key_spki_der);

  EXPECT_TRUE(encrypted_data.hmac_secret().empty());
  EXPECT_TRUE(encrypted_data.cred_blob().empty());
  EXPECT_TRUE(encrypted_data.large_blob().empty());
  EXPECT_EQ(encrypted_data.large_blob_uncompressed_size(), 0u);
}

TEST(PasskeyModelUtilsTest, GeneratePasskeyWithPRFAndEncryptSecrets) {
  std::vector<uint8_t> prf_input1;
  prf_input1.emplace_back('a');
  ExtensionInputData extension_input_data({prf_input1, std::nullopt});
  ExtensionOutputData extension_output_data;
  auto [passkey, public_key_spki_der] = GeneratePasskeyAndEncryptSecrets(
      kRpId, kTestUser, kTestKey, kTestKeyVersion, extension_input_data,
      &extension_output_data);
  EXPECT_EQ(passkey.sync_id().size(), 16u);
  EXPECT_EQ(passkey.credential_id().size(), 16u);
  EXPECT_EQ(passkey.rp_id(), kRpId);
  EXPECT_EQ(passkey.user_id(),
            std::string(reinterpret_cast<const char*>(kTestUser.id.data()),
                        kTestUser.id.size()));
  EXPECT_EQ(passkey.user_name(), kTestUser.name);
  EXPECT_EQ(passkey.user_display_name(), kTestUser.display_name);
  EXPECT_FALSE(passkey.third_party_payments_support());
  EXPECT_EQ(passkey.last_used_time_windows_epoch_micros(), 0u);
  EXPECT_GT(passkey.creation_time(), 0u);
  EXPECT_EQ(passkey.key_version(), kTestKeyVersion);

  // Filled in by the Sync model.
  EXPECT_TRUE(passkey.newly_shadowed_credential_ids().empty());

  EXPECT_TRUE(passkey.has_encrypted());
  sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted_data;
  ASSERT_TRUE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, /*device_authorization_key=*/{}, passkey, &encrypted_data));
  EXPECT_FALSE(encrypted_data.private_key().empty());
  auto ec_key = crypto::keypair::PrivateKey::FromPrivateKeyInfo(
      base::as_byte_span(encrypted_data.private_key()));
  ASSERT_TRUE(ec_key.has_value());
  ASSERT_TRUE(ec_key->IsEcP256());
  std::vector<uint8_t> ec_key_pub = ec_key->ToSubjectPublicKeyInfo();
  EXPECT_EQ(ec_key_pub, public_key_spki_der);

  EXPECT_EQ(encrypted_data.hmac_secret().size(), 32u);
  EXPECT_TRUE(encrypted_data.cred_blob().empty());
  EXPECT_TRUE(encrypted_data.large_blob().empty());
  EXPECT_EQ(encrypted_data.large_blob_uncompressed_size(), 0u);

  EXPECT_EQ(extension_output_data.prf_result.size(), 32u);
}

TEST(PasskeyModelUtilsTest, PRFInputsEmptyVSMissing) {
  std::vector<uint8_t> prf_empty_input, prf_non_empty_input;
  prf_non_empty_input.emplace_back('a');

  ExtensionOutputData extension_output_data;

  // First input empty, 2nd input missing, 1 PRF output.
  PRFInputData input_data_0X({prf_empty_input, std::nullopt});
  EXPECT_TRUE(input_data_0X.prf_input().input1.empty());
  EXPECT_FALSE(input_data_0X.prf_input().input2.has_value());
  GeneratePasskeyAndEncryptSecrets(kRpId, kTestUser, kTestKey, kTestKeyVersion,
                                   ExtensionInputData(input_data_0X),
                                   &extension_output_data);
  EXPECT_EQ(extension_output_data.prf_result.size(), 32u);

  // First input non empty, 2nd input missing, 1 PRF output.
  PRFInputData input_data_1X({prf_non_empty_input, std::nullopt});
  EXPECT_FALSE(input_data_1X.prf_input().input1.empty());
  EXPECT_FALSE(input_data_1X.prf_input().input2.has_value());
  GeneratePasskeyAndEncryptSecrets(kRpId, kTestUser, kTestKey, kTestKeyVersion,
                                   ExtensionInputData(input_data_1X),
                                   &extension_output_data);
  EXPECT_EQ(extension_output_data.prf_result.size(), 32u);

  // First input empty, 2nd input empty, 2 PRF outputs.
  PRFInputData input_data_00({prf_empty_input, prf_empty_input});
  EXPECT_TRUE(input_data_00.prf_input().input1.empty());
  EXPECT_TRUE(input_data_00.prf_input().input2.has_value());
  EXPECT_TRUE(input_data_00.prf_input().input2->empty());
  GeneratePasskeyAndEncryptSecrets(kRpId, kTestUser, kTestKey, kTestKeyVersion,
                                   ExtensionInputData(input_data_00),
                                   &extension_output_data);
  EXPECT_EQ(extension_output_data.prf_result.size(), 64u);

  // First input empty, 2nd input non empty, 2 PRF output2.
  PRFInputData input_data_01({prf_empty_input, prf_non_empty_input});
  EXPECT_TRUE(input_data_01.prf_input().input1.empty());
  EXPECT_TRUE(input_data_01.prf_input().input2.has_value());
  EXPECT_FALSE(input_data_01.prf_input().input2->empty());
  GeneratePasskeyAndEncryptSecrets(kRpId, kTestUser, kTestKey, kTestKeyVersion,
                                   ExtensionInputData(input_data_01),
                                   &extension_output_data);
  EXPECT_EQ(extension_output_data.prf_result.size(), 64u);

  // First input non empty, 2nd input empty, 2 PRF outputs.
  PRFInputData input_data_10({prf_non_empty_input, prf_empty_input});
  EXPECT_FALSE(input_data_10.prf_input().input1.empty());
  EXPECT_TRUE(input_data_10.prf_input().input2.has_value());
  EXPECT_TRUE(input_data_10.prf_input().input2->empty());
  GeneratePasskeyAndEncryptSecrets(kRpId, kTestUser, kTestKey, kTestKeyVersion,
                                   ExtensionInputData(input_data_10),
                                   &extension_output_data);
  EXPECT_EQ(extension_output_data.prf_result.size(), 64u);

  // First input non empty, 2nd input non empty, 2 PRF outputs.
  PRFInputData input_data_11({prf_non_empty_input, prf_non_empty_input});
  EXPECT_FALSE(input_data_11.prf_input().input1.empty());
  EXPECT_TRUE(input_data_11.prf_input().input2.has_value());
  EXPECT_FALSE(input_data_11.prf_input().input2->empty());
  GeneratePasskeyAndEncryptSecrets(kRpId, kTestUser, kTestKey, kTestKeyVersion,
                                   ExtensionInputData(input_data_11),
                                   &extension_output_data);
  EXPECT_EQ(extension_output_data.prf_result.size(), 64u);
}

TEST(PasskeyModelUtilsTest, ReturnsTrueForValidPasskey) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();

  EXPECT_TRUE(IsPasskeyValid(passkey));
  EXPECT_TRUE(IsGpmPasskeyValid(passkey));
}

TEST(PasskeyModelUtilsTest, ValidatesIncorrectSyncIdLength) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();
  passkey.set_sync_id(base::RandBytesAsString(kSyncIdLength - 1));

  EXPECT_FALSE(IsPasskeyValid(passkey));
  EXPECT_FALSE(IsGpmPasskeyValid(passkey));
}

TEST(PasskeyModelUtilsTest, ValidatesRpIdPresence) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();
  passkey.clear_rp_id();

  EXPECT_FALSE(IsPasskeyValid(passkey));
  EXPECT_FALSE(IsGpmPasskeyValid(passkey));
}

TEST(PasskeyModelUtilsTest, ValidatesCredentialIdLength) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();

  passkey.set_credential_id(
      base::RandBytesAsString(kCredentialIdMinLength - 1));
  EXPECT_FALSE(IsPasskeyValid(passkey));
  EXPECT_FALSE(IsGpmPasskeyValid(passkey));

  passkey.set_credential_id(
      base::RandBytesAsString(kCredentialIdMaxLength + 1));
  EXPECT_FALSE(IsPasskeyValid(passkey));
  EXPECT_FALSE(IsGpmPasskeyValid(passkey));

  passkey.set_credential_id(
      base::RandBytesAsString(kGpmCreatedCredentialIdLength + 1));
  EXPECT_TRUE(IsPasskeyValid(passkey));
  EXPECT_FALSE(IsGpmPasskeyValid(passkey));

  passkey.set_credential_id(
      base::RandBytesAsString(kGpmCreatedCredentialIdLength));
  EXPECT_TRUE(IsPasskeyValid(passkey));
  EXPECT_TRUE(IsGpmPasskeyValid(passkey));
}

TEST(PasskeyModelUtilsTest, ValidatesUserIdLength) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();

  passkey.set_user_id(base::RandBytesAsString(kUserIdMaxLength + 1));
  EXPECT_FALSE(IsPasskeyValid(passkey));
  EXPECT_FALSE(IsGpmPasskeyValid(passkey));
}

TEST(PasskeyModelUtilsTest, ValidatesMissingEncryptedFields) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();

  passkey.clear_private_key();
  passkey.clear_encrypted();
  EXPECT_FALSE(IsPasskeyValid(passkey));
  EXPECT_FALSE(IsGpmPasskeyValid(passkey));
}

TEST(PasskeyModelUtilsTest, GenerateEcSignature_ValidP256) {
  auto ec_p256_key = crypto::keypair::PrivateKey::GenerateEcP256();
  std::vector<uint8_t> pkcs8 = ec_p256_key.ToPrivateKeyInfo();
  const std::vector<uint8_t> data = {1, 2, 3, 4, 5};

  std::optional<std::vector<uint8_t>> signature =
      GenerateEcSignature(pkcs8, data);
  ASSERT_TRUE(signature.has_value());

  auto public_key = crypto::keypair::PublicKey::FromPrivateKey(ec_p256_key);
  EXPECT_TRUE(crypto::sign::Verify(crypto::sign::SignatureKind::ECDSA_SHA256,
                                   public_key, data, *signature));
}

TEST(PasskeyModelUtilsTest, GenerateEcSignature_NonP256Key) {
  const std::vector<uint8_t> data = {1, 2, 3, 4, 5};

  // RSA key should be rejected.
  auto rsa_key = crypto::keypair::PrivateKey::GenerateRsa2048();
  EXPECT_FALSE(
      GenerateEcSignature(rsa_key.ToPrivateKeyInfo(), data).has_value());

  // Ed25519 key should be rejected.
  auto ed25519_key = crypto::keypair::PrivateKey::GenerateEd25519();
  EXPECT_FALSE(
      GenerateEcSignature(ed25519_key.ToPrivateKeyInfo(), data).has_value());

  // EC P-384 key should be rejected (only P-256 is supported for ES256).
  auto ec_p384_key = crypto::keypair::PrivateKey::GenerateEcP384();
  EXPECT_FALSE(
      GenerateEcSignature(ec_p384_key.ToPrivateKeyInfo(), data).has_value());

  // Invalid bytes should be rejected.
  const std::vector<uint8_t> invalid_bytes = {0x01, 0x02, 0x03};
  EXPECT_FALSE(GenerateEcSignature(invalid_bytes, data).has_value());
}

// Test that the device authorization key is ignored when decrypting the
// `encrypted` case.
TEST(PasskeyModelUtilsTest, IgnoresDeviceAuthorizationKeyForEncryptedCase) {
  const sync_pb::WebauthnCredentialSpecifics_Encrypted secrets =
      CreateTestSecrets();
  sync_pb::WebauthnCredentialSpecifics passkey;
  ASSERT_TRUE(
      EncryptWebauthnCredentialSpecificsData(kTestKey, secrets, &passkey));

  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_TRUE(DecryptWebauthnCredentialSpecificsData(kTestKey, kWrongKey,
                                                     passkey, &out));
  EXPECT_THAT(out, EqualsProto(secrets));
}

// Test that the `security_domain_encrypted` case can't be decrypted if
// `kDeviceAuthorizationPasskeyDecryption` is disabled.
TEST(PasskeyModelUtilsTest,
     FailsToDecryptSecurityDomainPasskeyWithFeatureDisabled) {
  ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kDeviceAuthorizationPasskeyDecryption);
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, kTestDeviceAuthorizationKey,
      CreatePasskeyWithSecurityDomainEncrypted(kTestSecurityDomainEncrypted),
      &out));
}

// Test fixture with `kDeviceAuthorizationPasskeyDecryption` enabled.
class PasskeyModelUtilsDeviceAuthorizationTest : public testing::Test {
 private:
  ScopedFeatureList feature_list_{kDeviceAuthorizationPasskeyDecryption};
};

// Test decryption of the `security_domain_encrypted` case for
// `WebAuthnCredentialSpecifics.encrypted_data`.
TEST_F(PasskeyModelUtilsDeviceAuthorizationTest, DecryptsWithValidKeys) {
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_TRUE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, kTestDeviceAuthorizationKey,
      CreatePasskeyWithSecurityDomainEncrypted(kTestSecurityDomainEncrypted),
      &out));
  EXPECT_THAT(out, EqualsProto(CreateTestSecrets()));
}

// Test that the `security_domain_encrypted` case can't be decrypted without a
// device authorization key.
TEST_F(PasskeyModelUtilsDeviceAuthorizationTest,
       DecryptionFailsWithoutDeviceAuthorizationKey) {
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, /*device_authorization_key=*/{},
      CreatePasskeyWithSecurityDomainEncrypted(kTestSecurityDomainEncrypted),
      &out));
}

// Test that the `security_domain_encrypted` case can't be decrypted with the
// wrong trusted vault key.
TEST_F(PasskeyModelUtilsDeviceAuthorizationTest,
       DecryptionFailsWithWrongTrustedVaultKey) {
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kWrongKey, kTestDeviceAuthorizationKey,
      CreatePasskeyWithSecurityDomainEncrypted(kTestSecurityDomainEncrypted),
      &out));
}

// Test that the `security_domain_encrypted` case can't be decrypted with the
// wrong device authorization key.
TEST_F(PasskeyModelUtilsDeviceAuthorizationTest,
       DecryptionFailsWithWrongDeviceAuthorizationKey) {
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, kWrongKey,
      CreatePasskeyWithSecurityDomainEncrypted(kTestSecurityDomainEncrypted),
      &out));
}

// Test that decryption fails if `security_domain_encrypted` doesn't decrypt to
// a valid `SecurityDomainEncrypted` message.
TEST_F(PasskeyModelUtilsDeviceAuthorizationTest,
       DecryptionFailsWithInvalidOuterProto) {
  // An invalid message (a single `0xff` byte) encrypted with `kTestKey`.
  static constexpr auto kSecurityDomainEncrypted = base::span_from_cstring(
      "\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x68\xee\xbe\xca"
      "\x68\x6c\xff\x93\x2e\xaa\x46\x61\xa0\x71\x2f\x30\x3c");
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, kTestDeviceAuthorizationKey,
      CreatePasskeyWithSecurityDomainEncrypted(kSecurityDomainEncrypted),
      &out));
}

// Test that decryption fails if `security_domain_encrypted` decrypts to a
// `SecurityDomainEncrypted` message without `device_authorization_encrypted`.
TEST_F(PasskeyModelUtilsDeviceAuthorizationTest,
       DecryptionFailsWithEmptyOuterProto) {
  // An empty `SecurityDomainEncrypted` message encrypted with `kTestKey`.
  static constexpr auto kSecurityDomainEncrypted = base::span_from_cstring(
      "\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\xdd\x7c\xdf\xef"
      "\x9d\x6f\x60\x19\xe5\xaa\x0a\xa0\x69\x90\x07\x98");
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, kTestDeviceAuthorizationKey,
      CreatePasskeyWithSecurityDomainEncrypted(kSecurityDomainEncrypted),
      &out));
}

// Test that decryption fails if `device_authorization_encrypted` doesn't
// decrypt to a valid `Encrypted` message.
TEST_F(PasskeyModelUtilsDeviceAuthorizationTest,
       DecryptionFailsWithInvalidInnerProto) {
  // Like `kTestSecurityDomainEncrypted`, but with an invalid message (a single
  // `0xff` byte) instead of the `CreateTestSecrets()` secrets.
  static constexpr auto kSecurityDomainEncrypted = base::span_from_cstring(
      "\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x9d\xe6\xde\xd0"
      "\x51\x65\x77\x76\x24\x8e\x64\x4d\x12\x10\x20\xd2\xb1\x05\x9b\xeb"
      "\xdf\x19\x93\xbe\xac\xfe\x0b\xf1\x91\x09\xf0\x33\x78\x71\x85\x1f"
      "\x94\x2d\x2c\xe4\x95\xe5\xcc\x20\x64\x84\x53");
  sync_pb::WebauthnCredentialSpecifics_Encrypted out;
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(
      kTestKey, kTestDeviceAuthorizationKey,
      CreatePasskeyWithSecurityDomainEncrypted(kSecurityDomainEncrypted),
      &out));
}

}  // namespace
}  // namespace webauthn::passkey_model_utils

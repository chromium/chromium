// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_model_utils.h"

#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "crypto/ec_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn::passkey_model_utils {
namespace {

constexpr std::array<uint8_t, 32> kTestKey = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr int32_t kTestKeyVersion = 23;
constexpr std::string_view kRpId = "example.com";
static const PasskeyModel::UserEntity kTestUser(std::vector<uint8_t>{1, 2, 3},
                                                "user@example.com",
                                                "Example User");

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
    EXPECT_EQ(DecryptWebauthnCredentialSpecificsData(kTestKey, in, &out),
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
    EXPECT_EQ(DecryptWebauthnCredentialSpecificsData(kTestKey, in, &out),
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
  EXPECT_FALSE(DecryptWebauthnCredentialSpecificsData(kTestKey, in, &out));
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
  EXPECT_TRUE(
      DecryptWebauthnCredentialSpecificsData(kTestKey, encrypted, &decrypted));
  EXPECT_EQ(decrypted.private_key(), plain.private_key());
  EXPECT_EQ(decrypted.hmac_secret(), plain.hmac_secret());
  EXPECT_EQ(decrypted.cred_blob(), plain.cred_blob());
  EXPECT_EQ(decrypted.large_blob(), plain.large_blob());
  EXPECT_EQ(decrypted.large_blob_uncompressed_size(),
            plain.large_blob_uncompressed_size());
}

TEST(PasskeyModelUtilsTest, GeneratePasskeyAndEncryptSecrets) {
  auto [passkey, public_key_spki_der] = GeneratePasskeyAndEncryptSecrets(
      kRpId, kTestUser, kTestKey, kTestKeyVersion);
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
  ASSERT_TRUE(DecryptWebauthnCredentialSpecificsData(kTestKey, passkey,
                                                     &encrypted_data));
  EXPECT_FALSE(encrypted_data.private_key().empty());
  auto ec_key = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(
      base::as_bytes(base::make_span(encrypted_data.private_key())));
  EXPECT_NE(ec_key, nullptr);
  std::vector<uint8_t> ec_key_pub;
  EXPECT_TRUE(ec_key->ExportPublicKey(&ec_key_pub));
  EXPECT_EQ(ec_key_pub, public_key_spki_der);

  EXPECT_TRUE(encrypted_data.hmac_secret().empty());
  EXPECT_TRUE(encrypted_data.cred_blob().empty());
  EXPECT_TRUE(encrypted_data.large_blob().empty());
  EXPECT_EQ(encrypted_data.large_blob_uncompressed_size(), 0u);
}

}  // namespace
}  // namespace webauthn::passkey_model_utils

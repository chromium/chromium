// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "crypto/keypair.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::apple {

namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
constexpr char kTestApplicationTag[] = "test-application-tag";

constexpr SignatureVerifier::SignatureAlgorithm kAcceptableAlgos[] = {
    SignatureVerifier::ECDSA_SHA256};

// Tests behaviour that is unique to the macOS implementation of unexportable
// keys.
class UnexportableKeyMacTest : public testing::Test {
 protected:
  crypto::apple::ScopedFakeKeychainV2 scoped_fake_keychain_{
      kTestKeychainAccessGroup};

  const UnexportableKeyProvider::Config config_{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = kTestApplicationTag,
  };

  std::unique_ptr<UnexportableKeyProvider> provider_{
      GetUnexportableKeyProvider(config_)};
};

TEST_F(UnexportableKeyMacTest, SecureEnclaveAvailability) {
  for (bool available : {true, false}) {
    scoped_fake_keychain_.keychain()->set_secure_enclave_available(available);
    EXPECT_EQ(GetUnexportableKeyProvider(config_) != nullptr, available);
  }
}

TEST_F(UnexportableKeyMacTest, DeleteSigningKey) {
  std::unique_ptr<UnexportableSigningKey> key =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);
  ASSERT_TRUE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
  EXPECT_TRUE(
      provider_->AsStatefulUnexportableKeyProvider()->DeleteSigningKeySlowly(
          key->GetWrappedKey()));
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
  EXPECT_TRUE(scoped_fake_keychain_.keychain()->items().empty());
}

TEST_F(UnexportableKeyMacTest, DeleteUnknownSigningKey) {
  EXPECT_FALSE(
      provider_->AsStatefulUnexportableKeyProvider()->DeleteSigningKeySlowly(
          std::vector<uint8_t>{1, 2, 3}));
}

TEST_F(UnexportableKeyMacTest, DeleteSigningKeyWithWrongApplicationTag) {
  // Generate a key with the default provider.
  std::unique_ptr<UnexportableSigningKey> key =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);
  ASSERT_TRUE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));

  // Create a new provider with a different application tag.
  const UnexportableKeyProvider::Config new_config{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "wrong-application-tag",
  };
  std::unique_ptr<UnexportableKeyProvider> new_provider =
      GetUnexportableKeyProvider(new_config);
  ASSERT_TRUE(new_provider);

  // Deleting with the wrong provider should fail.
  EXPECT_FALSE(
      new_provider->AsStatefulUnexportableKeyProvider()->DeleteSigningKeySlowly(
          key->GetWrappedKey()));

  // The key should still exist and be loadable by the original provider.
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
  EXPECT_FALSE(scoped_fake_keychain_.keychain()->items().empty());

  // Deleting with the correct provider should succeed.
  EXPECT_TRUE(
      provider_->AsStatefulUnexportableKeyProvider()->DeleteSigningKeySlowly(
          key->GetWrappedKey()));
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
  EXPECT_TRUE(scoped_fake_keychain_.keychain()->items().empty());
}

TEST_F(UnexportableKeyMacTest, GetSecKeyRef) {
  ASSERT_TRUE(provider_);
  auto key = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);
  EXPECT_TRUE(key->GetSecKeyRef());
}

TEST_F(UnexportableKeyMacTest, GeneratedSpkiIsValid) {
  ASSERT_TRUE(provider_);
  auto key = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);

  const auto spki = key->GetSubjectPublicKeyInfo();
  const auto imported =
      crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(spki);
  ASSERT_TRUE(imported);
  EXPECT_TRUE(imported->IsEcP256());
}

}  // namespace

}  // namespace crypto::apple

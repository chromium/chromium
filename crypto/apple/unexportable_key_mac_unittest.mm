// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::apple {

namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
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
  EXPECT_TRUE(provider_->DeleteSigningKeySlowly(key->GetWrappedKey()));
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
  EXPECT_TRUE(scoped_fake_keychain_.keychain()->items().empty());
}

TEST_F(UnexportableKeyMacTest, DeleteUnknownSigningKey) {
  EXPECT_FALSE(
      provider_->DeleteSigningKeySlowly(std::vector<uint8_t>{1, 2, 3}));
}

TEST_F(UnexportableKeyMacTest, GetSecKeyRef) {
  ASSERT_TRUE(provider_);
  auto key = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);
  EXPECT_TRUE(key->GetSecKeyRef());
}

}  // namespace

}  // namespace crypto::apple

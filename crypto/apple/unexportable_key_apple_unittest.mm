// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "base/time/time_override.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "crypto/keypair.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::apple {

namespace {

using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// Defines a matcher that checks if an UnexportableSigningKey's wrapped key
// matches the expected bytes.
MATCHER_P(WrappedKeyEq, expected_key, "") {
  if (!arg) {
    *result_listener << "is null";
    return false;
  }
  return ExplainMatchResult(Eq(expected_key->GetWrappedKey()),
                            arg->GetWrappedKey(), result_listener);
}

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

TEST_F(UnexportableKeyMacTest, GetAllSigningKeys) {
  // Initially, there should be no keys.
  EXPECT_THAT(
      provider_->AsStatefulUnexportableKeyProvider()->GetAllSigningKeysSlowly(),
      Optional(IsEmpty()));

  // Create one key.
  std::unique_ptr<UnexportableSigningKey> key1 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_NE(key1, nullptr);

  EXPECT_THAT(
      provider_->AsStatefulUnexportableKeyProvider()->GetAllSigningKeysSlowly(),
      Optional(UnorderedElementsAre(WrappedKeyEq(key1.get()))));

  // Create a second key.
  std::unique_ptr<UnexportableSigningKey> key2 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_NE(key2, nullptr);

  EXPECT_THAT(
      provider_->AsStatefulUnexportableKeyProvider()->GetAllSigningKeysSlowly(),
      Optional(UnorderedElementsAre(WrappedKeyEq(key1.get()),
                                    WrappedKeyEq(key2.get()))));
}

TEST_F(UnexportableKeyMacTest, GetAllSigningKeysFiltersByTag) {
  ASSERT_TRUE(provider_);
  auto key = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);

  // Create a provider with a different application tag.
  const UnexportableKeyProvider::Config other_config{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "other-application-tag",
  };
  std::unique_ptr<UnexportableKeyProvider> other_provider =
      GetUnexportableKeyProvider(other_config);
  ASSERT_TRUE(other_provider);

  // Generate a key with the other provider.
  auto other_key = other_provider->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(other_key);

  // The original provider should still only see its own key.
  EXPECT_THAT(
      provider_->AsStatefulUnexportableKeyProvider()->GetAllSigningKeysSlowly(),
      Optional(UnorderedElementsAre(WrappedKeyEq(key.get()))));

  // The other provider should only see its own key.
  EXPECT_THAT(other_provider->AsStatefulUnexportableKeyProvider()
                  ->GetAllSigningKeysSlowly(),
              Optional(UnorderedElementsAre(WrappedKeyEq(other_key.get()))));
}

TEST_F(UnexportableKeyMacTest, GetAllSigningKeysPerformsPrefixMatching) {
  // 1. Create a key with the base tag "test-tag".
  UnexportableKeyProvider::Config config = config_;
  config.application_tag = "test-tag";
  std::unique_ptr<UnexportableKeyProvider> provider =
      GetUnexportableKeyProvider(config);
  ASSERT_TRUE(provider);
  auto key1 = provider->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key1);

  // 2. Create a key with a longer tag "test-tag.extension".
  // This should be visible to the first provider because "test-tag" is a
  // prefix of "test-tag.extension".
  UnexportableKeyProvider::Config extended_config = config_;
  extended_config.application_tag = "test-tag.extension";
  std::unique_ptr<UnexportableKeyProvider> extended_provider =
      GetUnexportableKeyProvider(extended_config);
  ASSERT_TRUE(extended_provider);
  auto key2 = extended_provider->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key2);

  // 3. Create a key with a completely different tag "other-tag".
  // This should NOT be visible to the first provider.
  UnexportableKeyProvider::Config other_config = config_;
  other_config.application_tag = "other-tag";
  std::unique_ptr<UnexportableKeyProvider> other_provider =
      GetUnexportableKeyProvider(other_config);
  ASSERT_TRUE(other_provider);
  auto key3 = other_provider->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key3);

  // 4. Verify that the base provider sees both its own key and the extended
  // key.
  EXPECT_THAT(
      provider->AsStatefulUnexportableKeyProvider()->GetAllSigningKeysSlowly(),
      Optional(UnorderedElementsAre(WrappedKeyEq(key1.get()),
                                    WrappedKeyEq(key2.get()))));

  // 5. Verify that the extended provider only sees its own key.
  // It should NOT see "test-tag" because "test-tag.extension" is not a
  // prefix of "test-tag".
  EXPECT_THAT(extended_provider->AsStatefulUnexportableKeyProvider()
                  ->GetAllSigningKeysSlowly(),
              Optional(UnorderedElementsAre(WrappedKeyEq(key2.get()))));
}

TEST_F(UnexportableKeyMacTest, FromWrappedSigningKeyRepairsKey) {
  // 1. Generate a key with the default provider (Tag A).
  auto key_original = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_original);
  auto wrapped_key = key_original->GetWrappedKey();

  // 2. Create a second provider with a different application tag (Tag B).
  const UnexportableKeyProvider::Config other_config{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "other-application-tag",
  };
  std::unique_ptr<UnexportableKeyProvider> other_provider =
      GetUnexportableKeyProvider(other_config);
  ASSERT_TRUE(other_provider);

  // 3. Initially, the second provider should not see any keys.
  EXPECT_THAT(other_provider->AsStatefulUnexportableKeyProvider()
                  ->GetAllSigningKeysSlowly(),
              Optional(IsEmpty()));

  // 4. Load the key using the second provider. This should trigger the
  //    repair/copy logic because the wrapped key (label) exists but the tag
  //    doesn't match.
  auto key_repaired = other_provider->FromWrappedSigningKeySlowly(wrapped_key);
  ASSERT_TRUE(key_repaired);
  // The wrapped key (which is the application label/hash) should be identical.
  EXPECT_THAT(key_repaired->GetWrappedKey(), Eq(wrapped_key));

  // 5. Verify the repaired key functions correctly (can sign).
  auto signature = key_repaired->SignSlowly(
      base::as_bytes(base::span_from_cstring("test data")));
  ASSERT_TRUE(signature);

  // 6. Verify that the key is now persisted for the second provider.
  //    GetAllSigningKeysSlowly filters by the provider's tag, so it should now
  //    find the newly created entry.
  EXPECT_THAT(other_provider->AsStatefulUnexportableKeyProvider()
                  ->GetAllSigningKeysSlowly(),
              Optional(UnorderedElementsAre(WrappedKeyEq(key_repaired.get()))));

  // 7. Verify the original key is still accessible to the first provider.
  //    The repair operation should copy the key, not move/steal it.
  EXPECT_THAT(
      provider_->AsStatefulUnexportableKeyProvider()->GetAllSigningKeysSlowly(),
      Optional(UnorderedElementsAre(WrappedKeyEq(key_original.get()))));
}

TEST_F(UnexportableKeyMacTest, FromWrappedSigningKeyForNonExistentKey) {
  // Verify that trying to load a completely non-existent key still returns null
  // and doesn't crash or create phantom entries.
  std::vector<uint8_t> bogus_wrapped_key = {1, 2, 3, 4, 5};
  auto key = provider_->FromWrappedSigningKeySlowly(bogus_wrapped_key);
  EXPECT_EQ(key, nullptr);
}

TEST_F(UnexportableKeyMacTest, GetKeyTag) {
  ASSERT_TRUE(provider_);
  auto key = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);
  EXPECT_EQ(key->AsStatefulUnexportableSigningKey()->GetKeyTag(),
            kTestApplicationTag);
}

TEST_F(UnexportableKeyMacTest, GetCreationTime) {
  ASSERT_TRUE(provider_);
  auto key = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);

  // Check that the time returned is the one set in the fake keychain.
  EXPECT_EQ(key->AsStatefulUnexportableSigningKey()->GetCreationTime(),
            base::Time::UnixEpoch());
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

TEST_F(UnexportableKeyMacTest, DeleteWrappedKeysSlowly) {
  // Generate three keys.
  std::unique_ptr<UnexportableSigningKey> key1 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key1);
  std::unique_ptr<UnexportableSigningKey> key2 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key2);
  std::unique_ptr<UnexportableSigningKey> key3 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key3);

  // Verify they exist.
  ASSERT_TRUE(provider_->FromWrappedSigningKeySlowly(key1->GetWrappedKey()));
  ASSERT_TRUE(provider_->FromWrappedSigningKeySlowly(key2->GetWrappedKey()));
  ASSERT_TRUE(provider_->FromWrappedSigningKeySlowly(key3->GetWrappedKey()));

  // Delete key1 and key3.
  std::optional<size_t> count =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteWrappedKeysSlowly({
          key1->GetWrappedKey(),
          key3->GetWrappedKey(),
      });
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 2u);

  // Verify key1 and key3 are gone, but key2 remains.
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key1->GetWrappedKey()));
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key2->GetWrappedKey()));
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key3->GetWrappedKey()));
  EXPECT_EQ(scoped_fake_keychain_.keychain()->items().size(), 1u);
}

TEST_F(UnexportableKeyMacTest, DeleteWrappedKeysSlowly_SingleKey) {
  // Generate three keys.
  std::unique_ptr<UnexportableSigningKey> key1 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key1);
  std::unique_ptr<UnexportableSigningKey> key2 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key2);
  std::unique_ptr<UnexportableSigningKey> key3 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key3);

  // Delete key2.
  std::optional<size_t> count =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteWrappedKeysSlowly(
          {key2->GetWrappedKey()});
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1u);

  // Verify key2 is gone, but key1 and key3 remain.
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key1->GetWrappedKey()));
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key2->GetWrappedKey()));
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key3->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteWrappedKeysSlowly_NonExistentKeys) {
  // Generate a key.
  std::unique_ptr<UnexportableSigningKey> key =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);

  // Try to delete a mix of existing and non-existing keys.
  std::vector<uint8_t> bogus_key1 = {1, 2, 3};
  std::vector<uint8_t> bogus_key2 = {4, 5, 6};
  std::optional<size_t> count =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteWrappedKeysSlowly({
          bogus_key1,
          key->GetWrappedKey(),
          bogus_key2,
      });
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1u);

  // Verify the real key is gone.
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteWrappedKeysSlowly_EmptyList) {
  // Generate a key.
  std::unique_ptr<UnexportableSigningKey> key =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);

  // Call delete with an empty list.
  std::optional<size_t> count =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteWrappedKeysSlowly(
          {});
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 0u);

  // Verify the key still exists.
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteWrappedKeysSlowly_PrefixMatching) {
  UnexportableKeyProvider::Config specific_config = config_;
  specific_config.application_tag += ".suffix";
  std::unique_ptr<UnexportableKeyProvider> specific_provider =
      GetUnexportableKeyProvider(specific_config);
  ASSERT_TRUE(specific_provider);

  auto key = specific_provider->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);
  std::vector<uint8_t> wrapped_key = key->GetWrappedKey();

  // `provider_`'s application tag is a prefix of the created key's tag, so
  // deletion should succeed.
  std::optional<size_t> count =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteWrappedKeysSlowly(
          {wrapped_key});
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1u);

  EXPECT_FALSE(specific_provider->FromWrappedSigningKeySlowly(wrapped_key));
  EXPECT_TRUE(scoped_fake_keychain_.keychain()->items().empty());
}

TEST_F(UnexportableKeyMacTest,
       DeleteWrappedKeysSlowly_ApplicationTagSeparation) {
  // Generate a key with the default provider.
  std::unique_ptr<UnexportableSigningKey> key =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);

  // Create a new provider with a different application tag.
  const UnexportableKeyProvider::Config new_config{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "wrong-application-tag",
  };
  std::unique_ptr<UnexportableKeyProvider> new_provider =
      GetUnexportableKeyProvider(new_config);
  ASSERT_TRUE(new_provider);

  // Deleting with the wrong provider should fail.
  std::optional<size_t> count_fail =
      new_provider->AsStatefulUnexportableKeyProvider()
          ->DeleteWrappedKeysSlowly({key->GetWrappedKey()});
  ASSERT_TRUE(count_fail.has_value());
  EXPECT_EQ(count_fail.value(), 0u);

  // The key should still exist.
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));

  // Deleting with the correct provider should succeed.
  std::optional<size_t> count_success =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteWrappedKeysSlowly(
          {key->GetWrappedKey()});
  ASSERT_TRUE(count_success.has_value());
  EXPECT_EQ(count_success.value(), 1u);
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteSigningKeysSlowly_KeyObjects) {
  // Generate two keys.
  auto key1 = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key1);
  auto key2 = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key2);

  auto* stateful_key1 = key1->AsStatefulUnexportableSigningKey();
  ASSERT_TRUE(stateful_key1);

  // Delete key1 using the new overload.
  std::optional<size_t> count =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteSigningKeysSlowly(
          {stateful_key1});
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1u);

  // Verify key1 is gone, but key2 remains.
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key1->GetWrappedKey()));
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key2->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteSigningKeysSlowly_PrecisionCollision) {
  // Generate a key with the default provider (Tag A).
  auto key_a = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_a);
  auto wrapped_key = key_a->GetWrappedKey();

  // Create a second provider with a different application tag (Tag B).
  const UnexportableKeyProvider::Config config_b{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "other-application-tag",
  };
  auto provider_b = GetUnexportableKeyProvider(config_b);
  ASSERT_TRUE(provider_b);

  // Load the key using the second provider. This triggers the repair logic,
  // creating a SECOND entry in the keychain with the SAME label but Tag B.
  auto key_b = provider_b->FromWrappedSigningKeySlowly(wrapped_key);
  ASSERT_TRUE(key_b);

  // Verify there are now two items in the fake keychain.
  ASSERT_EQ(scoped_fake_keychain_.keychain()->items().size(), 2u);

  // Delete using the Tag A key object.
  std::optional<size_t> count =
      provider_->AsStatefulUnexportableKeyProvider()->DeleteSigningKeysSlowly(
          {key_a->AsStatefulUnexportableSigningKey()});
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1u);

  // Verify only one item remains in the keychain.
  EXPECT_EQ(scoped_fake_keychain_.keychain()->items().size(), 1u);

  // Verify that provider A no longer sees any keys.
  // We use GetAllSigningKeysSlowly() here because it only returns keys
  // matching Tag A and does NOT trigger the repair logic.
  EXPECT_THAT(
      provider_->AsStatefulUnexportableKeyProvider()->GetAllSigningKeysSlowly(),
      Optional(IsEmpty()));

  // Verify that provider B still sees its key.
  EXPECT_THAT(provider_b->AsStatefulUnexportableKeyProvider()
                  ->GetAllSigningKeysSlowly(),
              Optional(SizeIs(1)));
}

TEST_F(UnexportableKeyMacTest, DeleteSigningKeysSlowly_TagMismatch) {
  auto key = provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key);

  // Create a provider with a different tag.
  const UnexportableKeyProvider::Config config_other{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "different-tag",
  };
  auto provider_other = GetUnexportableKeyProvider(config_other);

  // Attempt to delete the key using the wrong provider.
  // It should find 0 matching items because the tags don't match.
  std::optional<size_t> count =
      provider_other->AsStatefulUnexportableKeyProvider()
          ->DeleteSigningKeysSlowly({key->AsStatefulUnexportableSigningKey()});
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 0u);

  // Verify the key still exists for the original provider.
  EXPECT_TRUE(provider_->FromWrappedSigningKeySlowly(key->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteAllSigningKeys) {
  // Generate two keys.
  std::unique_ptr<UnexportableSigningKey> key1 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key1);
  std::unique_ptr<UnexportableSigningKey> key2 =
      provider_->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key2);

  // Verify they exist.
  ASSERT_TRUE(provider_->FromWrappedSigningKeySlowly(key1->GetWrappedKey()));
  ASSERT_TRUE(provider_->FromWrappedSigningKeySlowly(key2->GetWrappedKey()));

  // Delete all.
  std::optional<size_t> count = provider_->AsStatefulUnexportableKeyProvider()
                                    ->DeleteAllSigningKeysSlowly();
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 2u);

  // Verify they are gone.
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key1->GetWrappedKey()));
  EXPECT_FALSE(provider_->FromWrappedSigningKeySlowly(key2->GetWrappedKey()));
  EXPECT_TRUE(scoped_fake_keychain_.keychain()->items().empty());
}

TEST_F(UnexportableKeyMacTest, DeleteAllSigningKeysPrefixMatching) {
  // Provider with tag "com.example"
  const UnexportableKeyProvider::Config config_prefix{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "com.example",
  };
  std::unique_ptr<UnexportableKeyProvider> provider_prefix =
      GetUnexportableKeyProvider(config_prefix);
  ASSERT_TRUE(provider_prefix);

  // Provider with tag "com.example.foo"
  const UnexportableKeyProvider::Config config_foo{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "com.example.foo",
  };
  std::unique_ptr<UnexportableKeyProvider> provider_foo =
      GetUnexportableKeyProvider(config_foo);
  ASSERT_TRUE(provider_foo);

  // Provider with tag "com.example.bar"
  const UnexportableKeyProvider::Config config_bar{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "com.example.bar",
  };
  std::unique_ptr<UnexportableKeyProvider> provider_bar =
      GetUnexportableKeyProvider(config_bar);
  ASSERT_TRUE(provider_bar);

  // Generate keys.
  auto key_foo = provider_foo->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_foo);
  auto key_bar = provider_bar->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_bar);

  // Use prefix provider to delete all starting with "com.example".
  std::optional<size_t> count =
      provider_prefix->AsStatefulUnexportableKeyProvider()
          ->DeleteAllSigningKeysSlowly();
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 2u);

  // Verify both are gone.
  EXPECT_FALSE(
      provider_foo->FromWrappedSigningKeySlowly(key_foo->GetWrappedKey()));
  EXPECT_FALSE(
      provider_bar->FromWrappedSigningKeySlowly(key_bar->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteAllSigningKeysPrefixMatchingSeparation) {
  // Provider with tag "com.example.foo"
  const UnexportableKeyProvider::Config config_foo{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "com.example.foo",
  };
  std::unique_ptr<UnexportableKeyProvider> provider_foo =
      GetUnexportableKeyProvider(config_foo);
  ASSERT_TRUE(provider_foo);

  // Provider with tag "com.example.bar"
  const UnexportableKeyProvider::Config config_bar{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "com.example.bar",
  };
  std::unique_ptr<UnexportableKeyProvider> provider_bar =
      GetUnexportableKeyProvider(config_bar);
  ASSERT_TRUE(provider_bar);

  // Generate keys.
  auto key_foo = provider_foo->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_foo);
  auto key_bar = provider_bar->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_bar);

  // Use foo provider to delete. Should only delete foo key.
  std::optional<size_t> count =
      provider_foo->AsStatefulUnexportableKeyProvider()
          ->DeleteAllSigningKeysSlowly();
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1u);

  // Verify foo is gone, bar remains.
  EXPECT_FALSE(
      provider_foo->FromWrappedSigningKeySlowly(key_foo->GetWrappedKey()));
  EXPECT_TRUE(
      provider_bar->FromWrappedSigningKeySlowly(key_bar->GetWrappedKey()));
}

TEST_F(UnexportableKeyMacTest, DeleteAllSigningKeysEmptyPrefix) {
  // Provider with empty tag. This should effectively act as a wildcard
  // and match all keys in the access group.
  const UnexportableKeyProvider::Config config_empty{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "",
  };
  std::unique_ptr<UnexportableKeyProvider> provider_empty =
      GetUnexportableKeyProvider(config_empty);
  ASSERT_TRUE(provider_empty);

  // Provider with a specific tag "com.example".
  const UnexportableKeyProvider::Config config_specific{
      .keychain_access_group = kTestKeychainAccessGroup,
      .application_tag = "com.example",
  };
  std::unique_ptr<UnexportableKeyProvider> provider_specific =
      GetUnexportableKeyProvider(config_specific);
  ASSERT_TRUE(provider_specific);

  // Generate a key with the specific tag "com.example".
  auto key_specific =
      provider_specific->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_specific);

  // Generate a key with the empty tag.
  auto key_empty = provider_empty->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(key_empty);

  // Verify both keys exist.
  ASSERT_TRUE(provider_specific->FromWrappedSigningKeySlowly(
      key_specific->GetWrappedKey()));
  ASSERT_TRUE(
      provider_empty->FromWrappedSigningKeySlowly(key_empty->GetWrappedKey()));

  // Delete using the empty-tag provider.
  // Since the empty string is a prefix of everything, the risk is too high to
  // accidentally delete all keys. Thus we should only delete keys that also
  // have an empty application_tag.
  std::optional<size_t> count =
      provider_empty->AsStatefulUnexportableKeyProvider()
          ->DeleteAllSigningKeysSlowly();
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1u);

  // Verify that the key with the empty tag is gone, but the other one still
  // exists.
  EXPECT_FALSE(
      provider_empty->FromWrappedSigningKeySlowly(key_empty->GetWrappedKey()));
  EXPECT_TRUE(provider_specific->FromWrappedSigningKeySlowly(
      key_specific->GetWrappedKey()));
}

}  // namespace

}  // namespace crypto::apple

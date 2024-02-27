// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_store.h"

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <optional>

#include "base/apple/foundation_util.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::fido::mac {
namespace {

using testing::UnorderedElementsAreArray;

static const PublicKeyCredentialUserEntity kUser({1, 2, 3},
                                                 "doe@example.com",
                                                 "John Doe");
constexpr char kRpId[] = "example.com";
constexpr char kOtherRpId[] = "foobar.com";

std::vector<PublicKeyCredentialDescriptor> AsDescriptors(
    base::span<Credential> credentials) {
  std::vector<PublicKeyCredentialDescriptor> descriptors;
  for (const auto& c : credentials) {
    descriptors.emplace_back(CredentialType::kPublicKey, c.credential_id);
  }
  return descriptors;
}

class CredentialStoreTest : public testing::Test {
 protected:
  std::vector<Credential> InsertCredentials(uint8_t count) {
    return InsertCredentialsForRp(kRpId, count);
  }

  Credential InsertLegacyCredential(CredentialMetadata::Version version,
                                    std::vector<uint8_t> user_id) {
    PublicKeyCredentialUserEntity user = kUser;
    user.id = std::move(user_id);
    return store_
        .CreateCredentialLegacyCredentialForTesting(
            version, kRpId, std::move(user),
            TouchIdCredentialStore::kNonDiscoverable)
        ->first;
  }

  std::vector<Credential> InsertCredentialsForRp(const std::string& rp_id,
                                                 uint8_t count) {
    std::vector<Credential> result;
    for (uint8_t i = 0; i < count; ++i) {
      PublicKeyCredentialUserEntity user = kUser;
      user.id = std::vector<uint8_t>({i});
      auto credential = store_.CreateCredential(
          rp_id, std::move(user), TouchIdCredentialStore::kDiscoverable);
      CHECK(credential) << "CreateCredential failed";
      result.emplace_back(std::move(credential->first));
    }
    return result;
  }

  AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  crypto::ScopedFakeAppleKeychainV2 keychain_{config_.keychain_access_group};
  TouchIdCredentialStore store_{config_};
};

TEST_F(CredentialStoreTest, CreateCredential) {
  auto result = store_.CreateCredential(kRpId, kUser,
                                        TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(result) << "CreateCredential failed";
  Credential credential = std::move(result->first);
  EXPECT_EQ(credential.credential_id.size(), 32u);
  EXPECT_TRUE(credential.private_key);
  base::apple::ScopedCFTypeRef<SecKeyRef> public_key =
      std::move(result->second);
  EXPECT_TRUE(public_key);
  EXPECT_EQ(
      credential.metadata,
      CredentialMetadata(CredentialMetadata::CurrentVersion(), kUser.id,
                         *kUser.name, *kUser.display_name, /*is_resident=*/true,
                         CredentialMetadata::SignCounter::kZero));
}

// FindCredentialsFromCredentialDescriptorList should find an inserted
// credential.
TEST_F(CredentialStoreTest, FindCredentialsFromCredentialDescriptorList_Basic) {
  std::vector<Credential> credentials = InsertCredentials(3);
  InsertCredentialsForRp("foo.com", 3);
  std::optional<std::list<Credential>> found =
      store_.FindCredentialsFromCredentialDescriptorList(
          kRpId, AsDescriptors(credentials));
  ASSERT_TRUE(found);
  EXPECT_THAT(*found, UnorderedElementsAreArray(credentials));

  found = store_.FindCredentialsFromCredentialDescriptorList(
      kRpId,
      std::vector<PublicKeyCredentialDescriptor>({PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey, /*id=*/{})}));
  EXPECT_TRUE(found && found->empty());

  found = store_.FindCredentialsFromCredentialDescriptorList(
      kOtherRpId, AsDescriptors(credentials));
  EXPECT_TRUE(found && found->empty());
}

// FindCredentialsFromCredentialDescriptorList with empty descriptor list should
// return an empty list.
TEST_F(CredentialStoreTest,
       FindCredentialsFromCredentialDescriptorList_ReturnEmpty) {
  std::vector<Credential> credentials = InsertCredentials(3);
  std::optional<std::list<Credential>> found =
      store_.FindCredentialsFromCredentialDescriptorList(
          kRpId, std::vector<PublicKeyCredentialDescriptor>());
  EXPECT_TRUE(found && found->empty());
}

// FindCredentialsFromCredentialDescriptorList should correctly return legacy
// credentials with IDs that have an old metadata version.
TEST_F(CredentialStoreTest,
       FindCredentialsFromCredentialDescriptorList_LegacyCredentials) {
  std::vector<Credential> credentials;
  for (const auto version :
       {CredentialMetadata::Version::kV0, CredentialMetadata::Version::kV1,
        CredentialMetadata::Version::kV2}) {
    credentials.emplace_back(InsertLegacyCredential(
        version,
        /*user_id=*/std::vector<uint8_t>({static_cast<uint8_t>(version)})));
  }

  std::optional<std::list<Credential>> found =
      store_.FindCredentialsFromCredentialDescriptorList(
          kRpId, AsDescriptors(credentials));
  ASSERT_TRUE(found);
  EXPECT_THAT(*found, UnorderedElementsAreArray(credentials));
}

// FindResidentCredentials should only return discoverable credentials.
TEST_F(CredentialStoreTest, FindResidentCredentials) {
  ASSERT_TRUE(store_.CreateCredential(
      kRpId, kUser, TouchIdCredentialStore::kNonDiscoverable));
  std::optional<std::list<Credential>> found =
      store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(found);
  EXPECT_EQ(found->size(), 0u);

  std::vector<Credential> credentials = InsertCredentials(10);
  found = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(found);
  EXPECT_THAT(*found, UnorderedElementsAreArray(credentials));
}

TEST_F(CredentialStoreTest, UpdateCredentialRecorded) {
  auto credential = store_.CreateCredential(
      kRpId, kUser, TouchIdCredentialStore::kNonDiscoverable);
  ASSERT_TRUE(credential);
  std::optional<std::list<Credential>> found =
      store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(found);
  EXPECT_EQ(found->size(), 0u);
  ASSERT_TRUE(
      store_.UpdateCredential(credential->first.credential_id, "new-username"));
}

}  // namespace
}  // namespace device::fido::mac

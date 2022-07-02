// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include "base/mac/foundation_util.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/fake_keychain.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device::fido::mac {
namespace {

using testing::UnorderedElementsAreArray;

static const PublicKeyCredentialUserEntity kUser({1, 2, 3},
                                                 "doe@example.com",
                                                 "John Doe",
                                                 absl::nullopt);
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
  ScopedFakeKeychain keychain_{config_.keychain_access_group};
  TouchIdCredentialStore store_{config_};
};

TEST_F(CredentialStoreTest, CreateCredential) {
  auto result = store_.CreateCredential(kRpId, kUser,
                                        TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(result) << "CreateCredential failed";
  Credential credential = std::move(result->first);
  EXPECT_EQ(credential.credential_id.size(), 59u);
  EXPECT_NE(credential.private_key, nullptr);
  base::ScopedCFTypeRef<SecKeyRef> public_key = std::move(result->second);
  EXPECT_NE(public_key, nullptr);
}

// FindCredentialsFromCredentialDescriptorList should find an inserted
// credential.
TEST_F(CredentialStoreTest, FindCredentialsFromCredentialDescriptorList_Basic) {
  std::vector<Credential> credentials = InsertCredentials(3);
  InsertCredentialsForRp("foo.com", 3);
  absl::optional<std::list<Credential>> found =
      store_.FindCredentialsFromCredentialDescriptorList(
          kRpId, AsDescriptors(credentials));
  ASSERT_TRUE(found);
  EXPECT_THAT(*found, UnorderedElementsAreArray(credentials));

  found = store_.FindCredentialsFromCredentialDescriptorList(
      kRpId,
      std::vector<PublicKeyCredentialDescriptor>({PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey, /*credential_id=*/{})}));
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
  absl::optional<std::list<Credential>> found =
      store_.FindCredentialsFromCredentialDescriptorList(
          kRpId, std::vector<PublicKeyCredentialDescriptor>());
  EXPECT_TRUE(found && found->empty());
}

// FindResidentCredentials should only return discoverable credentials.
TEST_F(CredentialStoreTest, FindResidentCredentials) {
  ASSERT_TRUE(store_.CreateCredential(
      kRpId, kUser, TouchIdCredentialStore::kNonDiscoverable));
  absl::optional<std::list<Credential>> found =
      store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(found);
  EXPECT_EQ(found->size(), 0u);

  std::vector<Credential> credentials = InsertCredentials(10);
  found = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(found);
  EXPECT_THAT(*found, UnorderedElementsAreArray(credentials));
}

}  // namespace
}  // namespace device::fido::mac
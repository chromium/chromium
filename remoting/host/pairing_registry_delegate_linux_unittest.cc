// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_linux.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kClientId1[] = "00000000-0000-0000-0000-000000000001";
constexpr char kClientId2[] = "00000000-0000-0000-0000-00000000000a";

}  // namespace

using protocol::PairingRegistry;

class PairingRegistryDelegateLinuxTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a temporary directory in order to get a unique name and use a
    // subdirectory to ensure that PairingRegistryDelegateLinux::Save() creates
    // the parent directory if it doesn't exist.
    base::CreateNewTempDirectory("chromoting-test", &temp_dir_);
    temp_registry_ = temp_dir_.Append("paired-clients");
  }

  void TearDown() override { base::DeletePathRecursively(temp_dir_); }

 protected:
  base::FilePath temp_dir_;
  base::FilePath temp_registry_;
};

TEST_F(PairingRegistryDelegateLinuxTest, SaveAndLoad) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/false);

  // Check that registry is initially empty.
  EXPECT_TRUE(delegate->LoadAll().empty());

  // Add a couple of pairings.
  PairingRegistry::Pairing pairing1(base::Time::Now(), "xxx", kClientId1,
                                    "xxx");
  PairingRegistry::Pairing pairing2(base::Time::Now(), "yyy", kClientId2,
                                    "yyy");
  EXPECT_TRUE(delegate->Save(pairing1));
  EXPECT_TRUE(delegate->Save(pairing2));

  // Verify that there are two pairings in the store now.
  EXPECT_EQ(delegate->LoadAll().size(), 2u);

  // Verify that they can be retrieved.
  EXPECT_EQ(delegate->Load(pairing1.client_id()), pairing1);
  EXPECT_EQ(delegate->Load(pairing2.client_id()), pairing2);

  // Delete the first pairing.
  EXPECT_TRUE(delegate->Delete(pairing1.client_id()));

  // Verify that there is only one pairing left.
  EXPECT_EQ(delegate->Load(pairing1.client_id()), PairingRegistry::Pairing());
  EXPECT_EQ(delegate->Load(pairing2.client_id()), pairing2);

  // Verify that the only value that left is |pairing2|.
  EXPECT_EQ(delegate->LoadAll().size(), 1u);
  base::ListValue pairings = delegate->LoadAll();
  ASSERT_TRUE(pairings[0].is_dict());
  EXPECT_EQ(PairingRegistry::Pairing::CreateFromValue(pairings[0].GetDict()),
            pairing2);

  // Delete the rest and verify.
  EXPECT_TRUE(delegate->DeleteAll());
  EXPECT_TRUE(delegate->LoadAll().empty());
}

// Verifies that the delegate is stateless by using two different instances.
TEST_F(PairingRegistryDelegateLinuxTest, Stateless) {
  auto save_delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/false);
  auto load_delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/false);

  PairingRegistry::Pairing pairing(base::Time::Now(), "xxx", kClientId1, "xxx");
  EXPECT_TRUE(save_delegate->Save(pairing));
  EXPECT_EQ(load_delegate->Load(pairing.client_id()), pairing);
}

TEST_F(PairingRegistryDelegateLinuxTest, SaveWithUnprivileged) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/true);

  PairingRegistry::Pairing pairing(base::Time::Now(), "client_name", kClientId1,
                                   "secret");
  EXPECT_TRUE(delegate->Save(pairing));

  // Verify both files exist.
  EXPECT_TRUE(base::PathExists(
      temp_registry_.Append(std::string(kClientId1) + ".json")));
  EXPECT_TRUE(base::PathExists(
      temp_registry_.Append(std::string(kClientId1) + ".unprivileged.json")));

  // Verify unprivileged file content.
  std::string unprivileged_json;
  ASSERT_TRUE(base::ReadFileToString(
      temp_registry_.Append(std::string(kClientId1) + ".unprivileged.json"),
      &unprivileged_json));
  std::optional<base::DictValue> unprivileged_dict =
      base::JSONReader::ReadDict(unprivileged_json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(unprivileged_dict);
  EXPECT_FALSE(unprivileged_dict->Find(PairingRegistry::kSharedSecretKey));
}

TEST_F(PairingRegistryDelegateLinuxTest, PrivilegedFilePermissions) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/false);

  PairingRegistry::Pairing pairing(base::Time::Now(), "client_name", kClientId1,
                                   "secret");
  EXPECT_TRUE(delegate->Save(pairing));

  base::FilePath privileged_pairing_file =
      temp_registry_.Append(std::string(kClientId1) + ".json");
  int permissions;
  ASSERT_TRUE(
      base::GetPosixFilePermissions(privileged_pairing_file, &permissions));
  EXPECT_EQ(permissions, base::FILE_PERMISSION_READ_BY_USER |
                             base::FILE_PERMISSION_WRITE_BY_USER);
}

TEST_F(PairingRegistryDelegateLinuxTest, LoadUnprivilegedFallback) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/true);

  PairingRegistry::Pairing pairing(base::Time::Now(), "client_name", kClientId1,
                                   "secret");
  base::DictValue pairing_value = pairing.ToValue();
  pairing_value.Remove(PairingRegistry::kSharedSecretKey);
  std::optional<std::string> unprivileged_pairing_json =
      base::WriteJson(pairing_value);
  ASSERT_TRUE(unprivileged_pairing_json.has_value());

  ASSERT_TRUE(base::CreateDirectory(temp_registry_));
  base::FilePath unprivileged_pairing_file =
      temp_registry_.Append(std::string(kClientId1) + ".unprivileged.json");
  ASSERT_TRUE(
      base::WriteFile(unprivileged_pairing_file, *unprivileged_pairing_json));

  // Set the privileged file to be unreadable to simulate access denied.
  base::FilePath privileged_pairing_file =
      temp_registry_.Append(std::string(kClientId1) + ".json");
  ASSERT_TRUE(base::WriteFile(privileged_pairing_file, "{}"));
  ASSERT_TRUE(base::SetPosixFilePermissions(privileged_pairing_file, 0000));

  PairingRegistry::Pairing loaded_pairing = delegate->Load(kClientId1);
  EXPECT_TRUE(loaded_pairing.is_valid());
  EXPECT_EQ(loaded_pairing.client_id(), kClientId1);
  EXPECT_EQ(loaded_pairing.shared_secret(), "");
}

TEST_F(PairingRegistryDelegateLinuxTest, LoadUnprivilegedNoFallbackIfDisabled) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/false);

  PairingRegistry::Pairing pairing(base::Time::Now(), "client_name", kClientId1,
                                   "secret");
  base::DictValue pairing_value = pairing.ToValue();
  pairing_value.Remove(PairingRegistry::kSharedSecretKey);
  std::optional<std::string> unprivileged_pairing_json =
      base::WriteJson(pairing_value);
  ASSERT_TRUE(unprivileged_pairing_json.has_value());

  ASSERT_TRUE(base::CreateDirectory(temp_registry_));
  base::FilePath unprivileged_pairing_file =
      temp_registry_.Append(std::string(kClientId1) + ".unprivileged.json");
  ASSERT_TRUE(
      base::WriteFile(unprivileged_pairing_file, *unprivileged_pairing_json));

  // Load should fail even if unprivileged file exists.
  PairingRegistry::Pairing loaded_pairing = delegate->Load(kClientId1);
  EXPECT_FALSE(loaded_pairing.is_valid());
}

TEST_F(PairingRegistryDelegateLinuxTest, LoadAllWithUnprivilegedFallback) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/true);

  // Pairing 1: Privileged is unreadable, should fall back to unprivileged.
  PairingRegistry::Pairing pairing1(base::Time::Now(), "name1", kClientId1,
                                    "secret1");
  base::DictValue pairing1_value = pairing1.ToValue();
  pairing1_value.Remove(PairingRegistry::kSharedSecretKey);
  std::optional<std::string> unprivileged1_json =
      base::WriteJson(pairing1_value);
  base::FilePath unprivileged1_file =
      temp_registry_.Append(std::string(kClientId1) + ".unprivileged.json");
  ASSERT_TRUE(base::CreateDirectory(temp_registry_));
  ASSERT_TRUE(base::WriteFile(unprivileged1_file, *unprivileged1_json));

  base::FilePath privileged1_file =
      temp_registry_.Append(std::string(kClientId1) + ".json");
  ASSERT_TRUE(base::WriteFile(privileged1_file, "{}"));
  ASSERT_TRUE(base::SetPosixFilePermissions(privileged1_file, 0000));

  // Pairing 2: Both exist and are readable, should prioritize privileged.
  PairingRegistry::Pairing pairing2(base::Time::Now(), "name2", kClientId2,
                                    "secret2");
  EXPECT_TRUE(delegate->Save(pairing2));
  base::DictValue pairing2_value = pairing2.ToValue();
  pairing2_value.Remove(PairingRegistry::kSharedSecretKey);
  std::optional<std::string> unprivileged2_json =
      base::WriteJson(pairing2_value);
  base::FilePath unprivileged2_file =
      temp_registry_.Append(std::string(kClientId2) + ".unprivileged.json");
  ASSERT_TRUE(base::WriteFile(unprivileged2_file, *unprivileged2_json));

  base::ListValue pairings = delegate->LoadAll();
  EXPECT_EQ(pairings.size(), 2u);

  bool found_id1 = false;
  bool found_id2 = false;
  for (const auto& pairing_value : pairings) {
    PairingRegistry::Pairing p =
        PairingRegistry::Pairing::CreateFromValue(pairing_value.GetDict());
    if (p.client_id() == kClientId1) {
      found_id1 = true;
      EXPECT_EQ(p.shared_secret(), "");
    } else if (p.client_id() == kClientId2) {
      found_id2 = true;
      EXPECT_EQ(p.shared_secret(), "secret2");
    }
  }
  EXPECT_TRUE(found_id1);
  EXPECT_TRUE(found_id2);
}

TEST_F(PairingRegistryDelegateLinuxTest, InvalidClientId) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/true);

  PairingRegistry::Pairing bad_pairing(base::Time::Now(), "xxx", "invalid_uuid",
                                       "xxx");
  EXPECT_FALSE(delegate->Save(bad_pairing));
  EXPECT_FALSE(base::PathExists(temp_registry_.Append("invalid_uuid.json")));
  EXPECT_FALSE(base::PathExists(
      temp_registry_.Append("invalid_uuid.unprivileged.json")));

  EXPECT_FALSE(delegate->Load("invalid_uuid").is_valid());
  EXPECT_FALSE(delegate->Delete("invalid_uuid"));
}

TEST_F(PairingRegistryDelegateLinuxTest, LoadAllFiltersInvalidFilenames) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/true);

  ASSERT_TRUE(base::CreateDirectory(temp_registry_));

  // Write a valid pairing.
  PairingRegistry::Pairing pairing(base::Time::Now(), "name", kClientId1,
                                   "secret");
  EXPECT_TRUE(delegate->Save(pairing));

  // Write an invalid pairing file (invalid UUID).
  ASSERT_TRUE(
      base::WriteFile(temp_registry_.Append("invalid_uuid.json"), "{}"));

  // Write a non-canonical pairing file (uppercase UUID) with valid content.
  PairingRegistry::Pairing pairing2(base::Time::Now(), "name2", kClientId2,
                                    "secret2");
  base::DictValue pairing2_value = pairing2.ToValue();
  std::optional<std::string> pairing2_json = base::WriteJson(pairing2_value);
  ASSERT_TRUE(pairing2_json.has_value());
  std::string uppercase_uuid = base::ToUpperASCII(std::string(kClientId2));
  ASSERT_TRUE(base::WriteFile(temp_registry_.Append(uppercase_uuid + ".json"),
                              *pairing2_json));

  // Write a file with wrong extension.
  ASSERT_TRUE(base::WriteFile(
      temp_registry_.Append(std::string(kClientId2) + ".txt"), "{}"));

  // Load all and verify only the valid lowercase one is returned.
  base::ListValue pairings = delegate->LoadAll();
  EXPECT_EQ(pairings.size(), 1u);
  ASSERT_TRUE(pairings[0].is_dict());
  EXPECT_EQ(PairingRegistry::Pairing::CreateFromValue(pairings[0].GetDict())
                .client_id(),
            kClientId1);
}

TEST_F(PairingRegistryDelegateLinuxTest, LoadPayloadConsistencyCheck) {
  auto delegate = std::make_unique<PairingRegistryDelegateLinux>(
      temp_registry_, /*use_unprivileged_file=*/false);

  ASSERT_TRUE(base::CreateDirectory(temp_registry_));

  // Create a pairing with client ID 1.
  PairingRegistry::Pairing pairing(base::Time::Now(), "name", kClientId1,
                                   "secret");
  base::DictValue pairing_value = pairing.ToValue();
  // But change the internal ID to client ID 2.
  pairing_value.Set(PairingRegistry::kClientIdKey, kClientId2);

  std::optional<std::string> pairing_json = base::WriteJson(pairing_value);
  ASSERT_TRUE(pairing_json.has_value());

  // Write to the file for client ID 1.
  base::FilePath file_path =
      temp_registry_.Append(std::string(kClientId1) + ".json");
  ASSERT_TRUE(base::WriteFile(file_path, *pairing_json));

  // Attempt to load client ID 1. It should fail because of mismatched payload.
  PairingRegistry::Pairing loaded_pairing = delegate->Load(kClientId1);
  EXPECT_FALSE(loaded_pairing.is_valid());
}

}  // namespace remoting

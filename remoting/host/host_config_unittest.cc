// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kHostOwnerJid[] = "obfuscated_value@id.talk.google.com";
constexpr char kHostOwnerEmail[] = "host_owner@gmail.com";
constexpr char kServiceAccountEmail[] =
    "aaaaaaaabbbbccccddddeeeeeeeeeeee@chromoting.gserviceaccount.com";
constexpr char kHostId[] = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
constexpr char kHostName[] = "TEST_MACHINE_NAME";
constexpr char kRefreshToken[] = "TEST_REFRESH_TOKEN";
constexpr char kPrivateKey[] = "TEST_PRIVATE_KEY";

constexpr char kNewRefreshToken[] = "NEW_REFRESH_TOKEN";

auto kBaseConfig = base::Value::Dict()
                       .Set(kOAuthRefreshTokenConfigPath, kRefreshToken)
                       .Set(kHostIdConfigPath, kHostId)
                       .Set(kHostNameConfigPath, kHostName)
                       .Set(kPrivateKeyConfigPath, kPrivateKey);

auto kTestConfig = base::Value::Dict(kBaseConfig.Clone())
                       .Set(kHostOwnerConfigPath, kHostOwnerEmail)
                       .Set(kServiceAccountConfigPath, kServiceAccountEmail);

auto kLegacyTestConfig =
    base::Value::Dict(kBaseConfig.Clone())
        .Set(kHostOwnerConfigPath, kHostOwnerJid)
        .Set(kDeprecatedHostOwnerEmailConfigPath, kHostOwnerEmail)
        .Set(kDeprecatedXmppLoginConfigPath, kServiceAccountEmail);

void WriteTestFile(const base::FilePath& filename,
                   const base::Value::Dict& file_contents) {
  auto json = base::WriteJson(file_contents);
  ASSERT_TRUE(json.has_value());
  base::WriteFile(filename, *json);
}

}  // namespace

class HostConfigTest : public ::testing::TestWithParam<base::Value::Dict*> {
 public:
  HostConfigTest(const HostConfigTest&) = delete;
  HostConfigTest& operator=(const HostConfigTest&) = delete;

 protected:
  HostConfigTest() = default;

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir test_dir_;
};

TEST(HostConfigTest, InvalidFile) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath non_existent_file =
      test_dir.GetPath().AppendASCII("non_existent.json");
  ASSERT_FALSE(HostConfigFromJsonFile(non_existent_file));
}

TEST_P(HostConfigTest, ReadConfigFromFile) {
  // Write file directly using base libraries.
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  base::FilePath test_file_path = test_dir_.GetPath().AppendASCII("read.json");
  WriteTestFile(test_file_path, *GetParam());

  // Read the config from the test file.
  auto target(HostConfigFromJsonFile(test_file_path));
  ASSERT_TRUE(target.has_value());

  // Verify the expected values.
  std::string* value = target->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostOwnerEmail);
  value = target->FindString(kServiceAccountConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kServiceAccountEmail);
  value = target->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kRefreshToken);
  value = target->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostId);
  value = target->FindString(kHostNameConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostName);
  value = target->FindString(kPrivateKeyConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kPrivateKey);

  // Verify deprecated values are not present.
  value = target->FindString(kDeprecatedXmppLoginConfigPath);
  EXPECT_EQ(value, nullptr);
  value = target->FindString(kDeprecatedHostOwnerEmailConfigPath);
  EXPECT_EQ(value, nullptr);

  // Verify non-existent values are not present.
  value = target->FindString("non_existent_value");
  EXPECT_EQ(value, nullptr);
}

TEST_P(HostConfigTest, WriteConfigToFile) {
  // Write file directly using base libraries.
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  base::FilePath test_file_path = test_dir_.GetPath().AppendASCII("write.json");
  WriteTestFile(test_file_path, *GetParam());

  // Read from the test file.
  auto target(HostConfigFromJsonFile(test_file_path));
  ASSERT_TRUE(target.has_value());

  // Modify a value.
  target->Set(kOAuthRefreshTokenConfigPath, kNewRefreshToken);
  ASSERT_TRUE(HostConfigToJsonFile(*target, test_file_path));

  // Now read the file again and check that the value has been written.
  auto reader(HostConfigFromJsonFile(test_file_path));
  ASSERT_TRUE(reader.has_value());

  // Verify the update value was persisted.
  std::string* value = target->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kNewRefreshToken);

  // Verify the rest of the expected values.
  value = target->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostOwnerEmail);
  value = target->FindString(kServiceAccountConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kServiceAccountEmail);
  value = target->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostId);
  value = target->FindString(kHostNameConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostName);
  value = target->FindString(kPrivateKeyConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kPrivateKey);

  // Verify deprecated values are not present.
  value = target->FindString(kDeprecatedXmppLoginConfigPath);
  EXPECT_EQ(value, nullptr);
  value = target->FindString(kDeprecatedHostOwnerEmailConfigPath);
  EXPECT_EQ(value, nullptr);

  // Verify non-existent values are not present.
  value = target->FindString("non_existent_value");
  EXPECT_EQ(value, nullptr);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HostConfigTest,
                         testing::Values(&kTestConfig, &kLegacyTestConfig));

}  // namespace remoting

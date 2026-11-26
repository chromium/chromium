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

auto kBaseConfig = base::DictValue()
                       .Set(kOAuthRefreshTokenConfigPath, kRefreshToken)
                       .Set(kHostIdConfigPath, kHostId)
                       .Set(kHostNameConfigPath, kHostName)
                       .Set(kPrivateKeyConfigPath, kPrivateKey);

auto kTestConfig = base::DictValue(kBaseConfig.Clone())
                       .Set(kHostOwnerConfigPath, kHostOwnerEmail)
                       .Set(kServiceAccountConfigPath, kServiceAccountEmail);

auto kLegacyTestConfig =
    base::DictValue(kBaseConfig.Clone())
        .Set(kHostOwnerConfigPath, kHostOwnerJid)
        .Set(kDeprecatedHostOwnerEmailConfigPath, kHostOwnerEmail)
        .Set(kDeprecatedXmppLoginConfigPath, kServiceAccountEmail);

void WriteTestFile(const base::FilePath& filename,
                   const base::DictValue& file_contents) {
  auto json = base::WriteJson(file_contents);
  ASSERT_TRUE(json.has_value());
  base::WriteFile(filename, *json);
}

}  // namespace

class HostConfigTest : public ::testing::TestWithParam<base::DictValue*> {
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
  EXPECT_EQ(target->FindStr(kHostOwnerConfigPath), kHostOwnerEmail);
  EXPECT_EQ(target->FindStr(kServiceAccountConfigPath), kServiceAccountEmail);
  EXPECT_EQ(target->FindStr(kOAuthRefreshTokenConfigPath), kRefreshToken);
  EXPECT_EQ(target->FindStr(kHostIdConfigPath), kHostId);
  EXPECT_EQ(target->FindStr(kHostNameConfigPath), kHostName);
  EXPECT_EQ(target->FindStr(kPrivateKeyConfigPath), kPrivateKey);

  // Verify deprecated values are not present.
  EXPECT_EQ(target->FindStr(kDeprecatedXmppLoginConfigPath), std::nullopt);
  EXPECT_EQ(target->FindStr(kDeprecatedHostOwnerEmailConfigPath), std::nullopt);

  // Verify non-existent values are not present.
  EXPECT_EQ(target->FindStr("non_existent_value"), std::nullopt);
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
  EXPECT_EQ(target->FindStr(kOAuthRefreshTokenConfigPath), kNewRefreshToken);

  // Verify the rest of the expected values.
  EXPECT_EQ(target->FindStr(kHostOwnerConfigPath), kHostOwnerEmail);
  EXPECT_EQ(target->FindStr(kServiceAccountConfigPath), kServiceAccountEmail);
  EXPECT_EQ(target->FindStr(kHostIdConfigPath), kHostId);
  EXPECT_EQ(target->FindStr(kHostNameConfigPath), kHostName);
  EXPECT_EQ(target->FindStr(kPrivateKeyConfigPath), kPrivateKey);

  // Verify deprecated values are not present.
  EXPECT_EQ(target->FindStr(kDeprecatedXmppLoginConfigPath), std::nullopt);
  EXPECT_EQ(target->FindStr(kDeprecatedHostOwnerEmailConfigPath), std::nullopt);

  // Verify non-existent values are not present.
  EXPECT_EQ(target->FindStr("non_existent_value"), std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HostConfigTest,
                         testing::Values(&kTestConfig, &kLegacyTestConfig));

}  // namespace remoting

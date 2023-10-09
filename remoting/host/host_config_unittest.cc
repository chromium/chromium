// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting {

namespace {

constexpr char kXmppLoginValue[] = "test@gmail.com";
constexpr char kHostIdValue[] = "TEST_HOST_ID";
constexpr char kHostNameValue[] = "TEST_MACHINE_NAME";
constexpr char kRefreshTokenValue[] = "TEST_REFRESH_TOKEN";
constexpr char kPrivateKeyValue[] = "TEST_PRIVATE_KEY";

constexpr char kNewRefreshTokenValue[] = "NEW_REFRESH_TOKEN";

auto kTestConfig = base::Value::Dict()
                       .Set(kXmppLoginConfigPath, kXmppLoginValue)
                       .Set(kOAuthRefreshTokenConfigPath, kRefreshTokenValue)
                       .Set(kHostIdConfigPath, kHostIdValue)
                       .Set(kHostNameConfigPath, kHostNameValue)
                       .Set(kPrivateKeyConfigPath, kPrivateKeyValue);

void WriteTestFile(const base::FilePath& filename,
                   const base::Value::Dict& file_contents) {
  auto json = base::WriteJson(file_contents);
  ASSERT_TRUE(json.has_value());
  base::WriteFile(filename, json.value());
}

}  // namespace

class HostConfigTest : public testing::Test {
 public:
  HostConfigTest(const HostConfigTest&) = delete;
  HostConfigTest& operator=(const HostConfigTest&) = delete;

 protected:
  HostConfigTest() = default;

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir test_dir_;
};

TEST_F(HostConfigTest, InvalidFile) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  base::FilePath non_existent_file =
      test_dir_.GetPath().AppendASCII("non_existent.json");
  EXPECT_FALSE(HostConfigFromJsonFile(non_existent_file));
}

TEST_F(HostConfigTest, Read) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  base::FilePath test_file_path = test_dir_.GetPath().AppendASCII("read.json");
  WriteTestFile(test_file_path, kTestConfig);
  absl::optional<base::Value::Dict> target(
      HostConfigFromJsonFile(test_file_path));
  ASSERT_TRUE(target.has_value());

  std::string* value = target->FindString(kXmppLoginConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kXmppLoginValue);
  value = target->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kRefreshTokenValue);
  value = target->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostIdValue);
  value = target->FindString(kHostNameConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostNameValue);
  value = target->FindString(kPrivateKeyConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kPrivateKeyValue);

  value = target->FindString("non_existent_value");
  EXPECT_EQ(value, nullptr);
}

TEST_F(HostConfigTest, Write) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

  base::FilePath test_file_path = test_dir_.GetPath().AppendASCII("write.json");
  WriteTestFile(test_file_path, kTestConfig);
  absl::optional<base::Value::Dict> target(
      HostConfigFromJsonFile(test_file_path));
  ASSERT_TRUE(target.has_value());

  target->Set(kOAuthRefreshTokenConfigPath, kNewRefreshTokenValue);
  ASSERT_TRUE(HostConfigToJsonFile(*target, test_file_path));

  // Now read the file again and check that the value has been written.
  absl::optional<base::Value> reader(HostConfigFromJsonFile(test_file_path));
  ASSERT_TRUE(reader);

  std::string* value = target->FindString(kXmppLoginConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kXmppLoginValue);
  value = target->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kNewRefreshTokenValue);
  value = target->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostIdValue);
  value = target->FindString(kHostNameConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kHostNameValue);
  value = target->FindString(kPrivateKeyConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kPrivateKeyValue);
}

}  // namespace remoting

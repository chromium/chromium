// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting {

namespace {

const char* kTestConfig =
    "{\n"
    "  \"xmpp_login\" : \"test@gmail.com\",\n"
    "  \"oauth_refresh_token\" : \"TEST_REFRESH_TOKEN\",\n"
    "  \"host_id\" : \"TEST_HOST_ID\",\n"
    "  \"host_name\" : \"TEST_MACHINE_NAME\",\n"
    "  \"private_key\" : \"TEST_PRIVATE_KEY\"\n"
    "}\n";

}  // namespace

class HostConfigTest : public testing::Test {
 public:
  HostConfigTest(const HostConfigTest&) = delete;
  HostConfigTest& operator=(const HostConfigTest&) = delete;

 protected:
  HostConfigTest() = default;

  static void WriteTestFile(const base::FilePath& filename) {
    base::WriteFile(filename, kTestConfig, std::strlen(kTestConfig));
  }

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
  base::FilePath test_file = test_dir_.GetPath().AppendASCII("read.json");
  WriteTestFile(test_file);
  absl::optional<base::Value::Dict> target(HostConfigFromJsonFile(test_file));
  ASSERT_TRUE(target.has_value());

  std::string* value = target->FindString(kXmppLoginConfigPath);
  EXPECT_EQ("test@gmail.com", *value);
  value = target->FindString(kOAuthRefreshTokenConfigPath);
  EXPECT_EQ("TEST_REFRESH_TOKEN", *value);
  value = target->FindString(kHostIdConfigPath);
  EXPECT_EQ("TEST_HOST_ID", *value);
  value = target->FindString(kHostNameConfigPath);
  EXPECT_EQ("TEST_MACHINE_NAME", *value);
  value = target->FindString(kPrivateKeyConfigPath);
  EXPECT_EQ("TEST_PRIVATE_KEY", *value);

  value = target->FindString("non_existent_value");
  EXPECT_EQ(nullptr, value);
}

TEST_F(HostConfigTest, Write) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

  base::FilePath test_file = test_dir_.GetPath().AppendASCII("write.json");
  WriteTestFile(test_file);
  absl::optional<base::Value::Dict> target(HostConfigFromJsonFile(test_file));
  ASSERT_TRUE(target.has_value());

  std::string new_refresh_token_value = "NEW_REFRESH_TOKEN";
  target->Set(kOAuthRefreshTokenConfigPath, new_refresh_token_value);
  ASSERT_TRUE(HostConfigToJsonFile(*target, test_file));

  // Now read the file again and check that the value has been written.
  absl::optional<base::Value> reader(HostConfigFromJsonFile(test_file));
  ASSERT_TRUE(reader);

  std::string* value = target->FindString(kXmppLoginConfigPath);
  EXPECT_EQ("test@gmail.com", *value);
  value = target->FindString(kOAuthRefreshTokenConfigPath);
  EXPECT_EQ(new_refresh_token_value, *value);
  value = target->FindString(kHostIdConfigPath);
  EXPECT_EQ("TEST_HOST_ID", *value);
  value = target->FindString(kHostNameConfigPath);
  EXPECT_EQ("TEST_MACHINE_NAME", *value);
  value = target->FindString(kPrivateKeyConfigPath);
  EXPECT_EQ("TEST_PRIVATE_KEY", *value);
}

}  // namespace remoting

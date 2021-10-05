// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  std::unique_ptr<base::DictionaryValue> target(
      HostConfigFromJsonFile(test_file));
  ASSERT_TRUE(target);

  std::string value;
  EXPECT_TRUE(target->GetString(kXmppLoginConfigPath, &value));
  EXPECT_EQ("test@gmail.com", value);
  EXPECT_TRUE(target->GetString(kOAuthRefreshTokenConfigPath, &value));
  EXPECT_EQ("TEST_REFRESH_TOKEN", value);
  EXPECT_TRUE(target->GetString(kHostIdConfigPath, &value));
  EXPECT_EQ("TEST_HOST_ID", value);
  EXPECT_TRUE(target->GetString(kHostNameConfigPath, &value));
  EXPECT_EQ("TEST_MACHINE_NAME", value);
  EXPECT_TRUE(target->GetString(kPrivateKeyConfigPath, &value));
  EXPECT_EQ("TEST_PRIVATE_KEY", value);

  EXPECT_FALSE(target->GetString("non_existent_value", &value));
}

TEST_F(HostConfigTest, Write) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

  base::FilePath test_file = test_dir_.GetPath().AppendASCII("write.json");
  WriteTestFile(test_file);
  std::unique_ptr<base::DictionaryValue> target(
      HostConfigFromJsonFile(test_file));
  ASSERT_TRUE(target);

  std::string new_refresh_token_value = "NEW_REFRESH_TOKEN";
  target->SetString(kOAuthRefreshTokenConfigPath, new_refresh_token_value);
  ASSERT_TRUE(HostConfigToJsonFile(*target, test_file));

  // Now read the file again and check that the value has been written.
  std::unique_ptr<base::DictionaryValue> reader(
      HostConfigFromJsonFile(test_file));
  ASSERT_TRUE(reader);

  std::string value;
  EXPECT_TRUE(reader->GetString(kXmppLoginConfigPath, &value));
  EXPECT_EQ("test@gmail.com", value);
  EXPECT_TRUE(reader->GetString(kOAuthRefreshTokenConfigPath, &value));
  EXPECT_EQ(new_refresh_token_value, value);
  EXPECT_TRUE(reader->GetString(kHostIdConfigPath, &value));
  EXPECT_EQ("TEST_HOST_ID", value);
  EXPECT_TRUE(reader->GetString(kHostNameConfigPath, &value));
  EXPECT_EQ("TEST_MACHINE_NAME", value);
  EXPECT_TRUE(reader->GetString(kPrivateKeyConfigPath, &value));
  EXPECT_EQ("TEST_PRIVATE_KEY", value);
}

}  // namespace remoting

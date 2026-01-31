// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_config.h"

#include <optional>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/test/scoped_command_line.h"
#include "google_apis/gaia/gaia_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::Eq;
using testing::IsNull;
using testing::Not;

const char kTestConfigContents[] = R"(
{
  "urls": {
    "test_url": {
      "url": "https://accounts.example.com/"
    }
  },
  "api_keys": {
    "test_api_key": "test_api_key_value"
  },
  "flags": {
    "test_flag": true
  }
})";

base::FilePath GetTestFilePath(const std::string& relative_path) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path)) {
    return base::FilePath();
  }
  return path.AppendASCII("google_apis")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("gaia")
      .AppendASCII(relative_path);
}

TEST(GaiaConfigTest, ShouldGetURLIfExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(
      kTestConfigContents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  GURL url;
  EXPECT_TRUE(config.GetURLIfExists("test_url", &url));
  EXPECT_THAT(url, Eq("https://accounts.example.com/"));
}

TEST(GaiaConfigTest, ShouldReturnNullIfURLDoesNotExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(
      kTestConfigContents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  GURL url;
  EXPECT_FALSE(config.GetURLIfExists("missing_url", &url));
}

TEST(GaiaConfigTest, ShouldGetAPIKeyIfExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(
      kTestConfigContents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  std::string api_key;
  EXPECT_TRUE(config.GetAPIKeyIfExists("test_api_key", &api_key));
  EXPECT_THAT(api_key, Eq("test_api_key_value"));
}

TEST(GaiaConfigTest, ShouldReturnNullIfAPIKeyDoesNotExist) {
  std::optional<base::Value> dict = base::JSONReader::Read(
      kTestConfigContents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  std::string api_key;
  EXPECT_FALSE(config.GetAPIKeyIfExists("missing_api_key", &api_key));
}

TEST(GaiaConfigTest, ShouldGetFlagIfExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(
      kTestConfigContents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  EXPECT_THAT(config.GetFlagIfExists("test_flag"), Eq(true));
}

TEST(GaiaConfigTest, ShouldReturnNulloptIfFlagDoesNotExist) {
  std::optional<base::Value> dict = base::JSONReader::Read(
      kTestConfigContents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  EXPECT_THAT(config.GetFlagIfExists("missing_flag"), Eq(std::nullopt));
}

TEST(GaiaConfigTest, ShouldGetFlagsFromFile) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("all_flags.json"));

  std::unique_ptr<GaiaConfig> config =
      GaiaConfig::CreateFromCommandLineForTesting(
          command_line.GetProcessCommandLine());
  ASSERT_THAT(config, Not(IsNull()));

  EXPECT_THAT(config->GetFlagIfExists("enable_issue_token_fetch"), Eq(true));
}

TEST(GaiaConfigTest, ShouldSerializeContentsToCommandLineSwitch) {
  std::optional<base::Value> dict = base::JSONReader::Read(
      kTestConfigContents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  GURL url;
  ASSERT_TRUE(config.GetURLIfExists("test_url", &url));
  ASSERT_THAT(url, Eq("https://accounts.example.com/"));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  config.SerializeContentsToCommandLineSwitch(&command_line);
  EXPECT_TRUE(command_line.HasSwitch(switches::kGaiaConfigContents));

  std::unique_ptr<GaiaConfig> deserialized_config =
      GaiaConfig::CreateFromCommandLineForTesting(&command_line);
  GURL deserialized_url;
  EXPECT_TRUE(
      deserialized_config->GetURLIfExists("test_url", &deserialized_url));
  EXPECT_THAT(deserialized_url, Eq("https://accounts.example.com/"));
}

TEST(GaiaConfigTest, ShouldRestoreGlobalInstance) {
  GaiaConfig* original_instance = GaiaConfig::GetInstance();

  {
    auto config_dict = base::DictValue().SetByDottedPath(
        "urls.test_url.url", "https://overridden.example.com/");
    auto scoped_override = GaiaConfig::SetScopedConfigForTesting(
        std::make_unique<GaiaConfig>(std::move(config_dict)));

    GaiaConfig* current_instance = GaiaConfig::GetInstance();
    ASSERT_THAT(current_instance, Not(IsNull()));
    ASSERT_THAT(current_instance, Not(Eq(original_instance)));

    GURL url;
    EXPECT_TRUE(current_instance->GetURLIfExists("test_url", &url));
    EXPECT_THAT(url, Eq("https://overridden.example.com/"));
  }

  EXPECT_THAT(GaiaConfig::GetInstance(), Eq(original_instance));
}

}  // namespace

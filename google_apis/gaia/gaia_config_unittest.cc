// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_config.h"

#include <optional>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "google_apis/gaia/gaia_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::Eq;
using testing::IsNull;

const char kTestConfigContents[] = R"(
{
  "urls": {
    "test_url": {
      "url": "https://accounts.example.com/"
    }
  },
  "api_keys": {
    "test_api_key": "test_api_key_value"
  }
})";

TEST(GaiaConfigTest, ShouldGetURLIfExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(kTestConfigContents);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  GURL url;
  EXPECT_TRUE(config.GetURLIfExists("test_url", &url));
  EXPECT_THAT(url, Eq("https://accounts.example.com/"));
}

TEST(GaiaConfigTest, ShouldReturnNullIfURLDoesNotExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(kTestConfigContents);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  GURL url;
  EXPECT_FALSE(config.GetURLIfExists("missing_url", &url));
}

TEST(GaiaConfigTest, ShouldGetAPIKeyIfExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(kTestConfigContents);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  std::string api_key;
  EXPECT_TRUE(config.GetAPIKeyIfExists("test_api_key", &api_key));
  EXPECT_THAT(api_key, Eq("test_api_key_value"));
}

TEST(GaiaConfigTest, ShouldReturnNullIfAPIKeyDoesNotExists) {
  std::optional<base::Value> dict = base::JSONReader::Read(kTestConfigContents);
  ASSERT_TRUE(dict.has_value());

  GaiaConfig config(std::move(dict->GetDict()));
  std::string api_key;
  EXPECT_FALSE(config.GetAPIKeyIfExists("missing_api_key", &api_key));
}

TEST(GaiaConfigTest, ShouldSerializeContentsToCommandLineSwitch) {
  std::optional<base::Value> dict = base::JSONReader::Read(kTestConfigContents);
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

}  // namespace

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/intelligence/test_ttc_api.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace ios::provider {

using ChromiumTTCApiTest = PlatformTest;

// Tests default construction and validity properties of TTCConfig.
TEST_F(ChromiumTTCApiTest, TestDefaultConfigProperties) {
  TTCConfig config;
  EXPECT_TRUE(config.empty());
  EXPECT_FALSE(config.is_valid());
  EXPECT_TRUE(config.system_instruction.empty());
  EXPECT_TRUE(config.model.empty());
  EXPECT_TRUE(config.voice_name.empty());
  EXPECT_TRUE(config.endpoint_url.empty());
  EXPECT_TRUE(config.api_key.empty());
}

// Tests validity checking when endpoint URL and model are present.
TEST_F(ChromiumTTCApiTest, TestConfigValidity) {
  TTCConfig invalid_config;
  invalid_config.endpoint_url = "wss://example.com";
  EXPECT_FALSE(invalid_config.is_valid());
  EXPECT_FALSE(invalid_config.empty());

  TTCConfig valid_config;
  valid_config.endpoint_url = "wss://example.com";
  valid_config.model = "models/test-model";
  EXPECT_TRUE(valid_config.is_valid());
  EXPECT_FALSE(valid_config.empty());
}

// Tests equality operator of TTCConfig.
TEST_F(ChromiumTTCApiTest, TestConfigEquality) {
  TTCConfig config1{
      .system_instruction = "instruction",
      .model = "model",
      .voice_name = "voice",
      .endpoint_url = "wss://example.com",
      .api_key = "key",
  };

  TTCConfig config2{
      .system_instruction = "instruction",
      .model = "model",
      .voice_name = "voice",
      .endpoint_url = "wss://example.com",
      .api_key = "key",
  };

  EXPECT_EQ(config1, config2);

  config2.api_key = "different-key";
  EXPECT_NE(config1, config2);
}

// Tests setting and resetting test overrides in test providers.
TEST_F(ChromiumTTCApiTest, TestMockConfigOverride) {
  test::ResetTTCConfigForTesting();

  TTCConfig default_mock = GetTTCConfig();
  EXPECT_TRUE(default_mock.is_valid());
  EXPECT_FALSE(default_mock.empty());

  TTCConfig custom_config{
      .system_instruction = "Custom instruction",
      .model = "models/custom-model",
      .voice_name = "CustomVoice",
      .endpoint_url = "wss://custom.example.com",
      .api_key = "custom-key",
  };

  test::SetTTCConfigForTesting(custom_config);
  EXPECT_EQ(GetTTCConfig(), custom_config);

  test::ResetTTCConfigForTesting();
  EXPECT_EQ(GetTTCConfig(), default_mock);
}

}  // namespace ios::provider

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_config.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "fuchsia_web/webengine/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCommandLineArgs[] = "command-line-args";

base::Value::Dict CreateConfigWithSwitchValue(std::string switch_name,
                                              std::string switch_value) {
  base::Value::Dict config_dict;
  base::Value::Dict args;
  args.Set(switch_name, switch_value);
  config_dict.Set(kCommandLineArgs, std::move(args));
  return config_dict;
}

}  // namespace

class WebEngineConfigTest : public testing::Test {
 public:
  WebEngineConfigTest() = default;
  ~WebEngineConfigTest() override = default;

  WebEngineConfigTest(const WebEngineConfigTest&) = delete;
  WebEngineConfigTest& operator=(const WebEngineConfigTest&) = delete;

  void SetUp() override {
    backup_field_trial_list_ = base::FieldTrialList::BackupInstanceForTesting();
  }

  void TearDown() override {
    base::FieldTrialList::RestoreInstanceForTesting(backup_field_trial_list_);
    backup_field_trial_list_ = nullptr;
  }

 private:
  base::FieldTrialList* backup_field_trial_list_ = nullptr;
};

TEST_F(WebEngineConfigTest, CommandLineArgs) {
  // Specify a configuration that sets valid args with valid strings.
  auto config = CreateConfigWithSwitchValue("renderer-process-limit", "0");
  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_EQ(command.GetSwitchValueASCII("renderer-process-limit"), "0");
}

TEST_F(WebEngineConfigTest, DisallowedCommandLineArgs) {
  // Specify a configuration that sets a disallowed command-line argument.
  auto config = CreateConfigWithSwitchValue("kittens-are-nice", "0");
  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_FALSE(command.HasSwitch("kittens-are-nice"));
}

TEST_F(WebEngineConfigTest, WronglyTypedCommandLineArgs) {
  base::Value::Dict config;

  // Specify a configuration that sets valid args with invalid value.
  base::Value::Dict args;
  args.Set("renderer-process-limit", false);
  config.Set(kCommandLineArgs, std::move(args));

  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(UpdateCommandLineFromConfigFile(config, &command));
}

TEST_F(WebEngineConfigTest, WithGoogleApiKeyValue) {
  constexpr char kDummyApiKey[] = "apikey123";
  auto config = CreateConfigWithSwitchValue("google-api-key", kDummyApiKey);
  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_EQ(command.GetSwitchValueASCII(switches::kGoogleApiKey), kDummyApiKey);
}
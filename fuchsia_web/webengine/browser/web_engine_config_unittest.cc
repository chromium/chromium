// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_config.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "content/public/common/content_switches.h"
#include "fuchsia_web/webengine/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCommandLineArgs[] = "command-line-args";

base::DictValue CreateConfigWithSwitchValue(std::string switch_name,
                                            std::string switch_value) {
  base::DictValue config_dict;
  base::DictValue args;
  args.Set(switch_name, switch_value);
  config_dict.Set(kCommandLineArgs, std::move(args));
  return config_dict;
}

enum class ConfigLocation {
  // Key nested inside the "command-line-args" dictionary in the config JSON.
  kCommandLineArgs,
  // Key in the root dictionary of the config JSON.
  kTopLevel,
};

struct ConfigArgTestParam {
  std::string test_name;
  ConfigLocation location;
  std::string arg_name;
  std::optional<std::string> arg_value;
  std::string expected_switch;
  std::optional<std::string> expected_value;
};

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
  raw_ptr<base::FieldTrialList> backup_field_trial_list_ = nullptr;
};

class WebEngineConfigArgumentsTest
    : public WebEngineConfigTest,
      public ::testing::WithParamInterface<ConfigArgTestParam> {};

TEST_P(WebEngineConfigArgumentsTest, SetArgument) {
  const ConfigArgTestParam& param = GetParam();

  base::DictValue config;
  if (param.location == ConfigLocation::kTopLevel) {
    ASSERT_TRUE(param.arg_value);
    config.Set(param.arg_name, *param.arg_value);
  } else {
    base::DictValue args;
    if (param.arg_value) {
      args.Set(param.arg_name, *param.arg_value);
    } else {
      args.Set(param.arg_name, base::Value());
    }
    config.Set(kCommandLineArgs, std::move(args));
  }

  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_TRUE(command.HasSwitch(param.expected_switch));
  if (param.expected_value) {
    EXPECT_EQ(command.GetSwitchValueASCII(param.expected_switch),
              *param.expected_value);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebEngineConfigArgumentsTest,
    testing::Values(
        ConfigArgTestParam{
            .test_name = "CommandLineArgs",
            .location = ConfigLocation::kCommandLineArgs,
            .arg_name = "renderer-process-limit",
            .arg_value = "0",
            .expected_switch = switches::kRendererProcessLimit,
            .expected_value = "0",
        },
        ConfigArgTestParam{
            .test_name = "WithGoogleApiKeyValue",
            .location = ConfigLocation::kCommandLineArgs,
            .arg_name = "google-api-key",
            .arg_value = "apikey123",
            .expected_switch = switches::kGoogleApiKey,
            .expected_value = "apikey123",
        },
        ConfigArgTestParam{
            .test_name = "UseSchedulerRolesCommandLineArg",
            .location = ConfigLocation::kCommandLineArgs,
            .arg_name = "use-scheduler-roles",
            .arg_value = "require",
            .expected_switch = switches::kUseSchedulerRoles,
            .expected_value = "require",
        },
        ConfigArgTestParam{
            .test_name = "UseSchedulerRolesTopLevel",
            .location = ConfigLocation::kTopLevel,
            .arg_name = "use-scheduler-roles",
            .arg_value = "require",
            .expected_switch = switches::kUseSchedulerRoles,
            .expected_value = "require",
        }),
    [](const testing::TestParamInfo<ConfigArgTestParam>& info) {
      return info.param.test_name;
    });

TEST_F(WebEngineConfigTest, DisallowedCommandLineArgs) {
  // Specify a configuration that sets a disallowed command-line argument.
  auto config = CreateConfigWithSwitchValue("kittens-are-nice", "0");
  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_FALSE(command.HasSwitch("kittens-are-nice"));
}

TEST_F(WebEngineConfigTest, WronglyTypedCommandLineArgs) {
  base::DictValue config;

  // Specify a configuration that sets valid args with invalid value.
  base::DictValue args;
  args.Set("renderer-process-limit", false);
  config.Set(kCommandLineArgs, std::move(args));

  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(UpdateCommandLineFromConfigFile(config, &command));
}

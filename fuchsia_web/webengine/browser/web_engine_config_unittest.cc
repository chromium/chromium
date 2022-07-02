// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_config.h"

#include "base/command_line.h"
#include "base/values.h"
#include "fuchsia_web/webengine/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCommandLineArgs[] = "command-line-args";

base::Value CreateConfigWithSwitchValue(std::string switch_name,
                                        std::string switch_value) {
  base::Value config_dict(base::Value::Type::DICTIONARY);
  base::Value args(base::Value::Type::DICTIONARY);
  args.SetStringKey(switch_name, switch_value);
  config_dict.SetKey(kCommandLineArgs, std::move(args));
  return config_dict;
}

}  // namespace

TEST(WebEngineConfig, CommandLineArgs) {
  // Specify a configuration that sets valid args with valid strings.
  auto config = CreateConfigWithSwitchValue("renderer-process-limit", "0");
  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_EQ(command.GetSwitchValueASCII("renderer-process-limit"), "0");
}

TEST(WebEngineConfig, DisallowedCommandLineArgs) {
  // Specify a configuration that sets a disallowed command-line argument.
  auto config = CreateConfigWithSwitchValue("kittens-are-nice", "0");
  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_FALSE(command.HasSwitch("kittens-are-nice"));
}

TEST(WebEngineConfig, WronglyTypedCommandLineArgs) {
  base::Value config(base::Value::Type::DICTIONARY);

  // Specify a configuration that sets valid args with invalid value.
  base::Value args(base::Value::Type::DICTIONARY);
  args.SetBoolKey("renderer-process-limit", false);
  config.SetKey(kCommandLineArgs, std::move(args));

  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(UpdateCommandLineFromConfigFile(config, &command));
}

TEST(WebEngineConfig, WithGoogleApiKeyValue) {
  constexpr char kDummyApiKey[] = "apikey123";
  auto config = CreateConfigWithSwitchValue("google-api-key", kDummyApiKey);
  base::CommandLine command(base::CommandLine::NO_PROGRAM);
  EXPECT_TRUE(UpdateCommandLineFromConfigFile(config, &command));
  EXPECT_EQ(command.GetSwitchValueASCII(switches::kGoogleApiKey), kDummyApiKey);
}
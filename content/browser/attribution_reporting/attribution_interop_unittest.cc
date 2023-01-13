// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_interop_parser.h"
#include "content/public/browser/attribution_config.h"
#include "content/public/browser/attribution_reporting.h"
#include "content/public/test/attribution_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ::testing::Optional;

constexpr char kDefaultConfigFileName[] = "default_config.json";

base::Value ReadJsonFromFile(const base::FilePath& path) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(path, &contents));
  return base::test::ParseJson(contents);
}

base::FilePath GetInputDir() {
  base::FilePath input_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &input_dir);
  return input_dir.AppendASCII(
      "content/test/data/attribution_reporting/interop");
}

std::vector<base::FilePath> GetInputs() {
  base::FilePath input_dir = GetInputDir();

  std::vector<base::FilePath> input_paths;

  base::FileEnumerator e(input_dir, /*recursive=*/false,
                         base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.json"));

  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    if (name.BaseName().MaybeAsASCII() == kDefaultConfigFileName) {
      continue;
    }

    input_paths.push_back(std::move(name));
  }

  return input_paths;
}

class AttributionInteropTest : public ::testing::TestWithParam<base::FilePath> {
};

// See //content/test/data/attribution_reporting/interop/README.md for the
// JSON schema.
TEST_P(AttributionInteropTest, HasExpectedOutput) {
  std::ostringstream error_stream;

  AttributionInteropParser parser(error_stream);

  AttributionConfig config;

  base::Value config_value =
      ReadJsonFromFile(GetInputDir().AppendASCII(kDefaultConfigFileName));

  bool is_config_valid =
      parser.ParseConfig(config_value, config, /*required=*/true);
  EXPECT_TRUE(is_config_valid) << error_stream.str();
  error_stream.str("");

  base::Value value = ReadJsonFromFile(GetParam());
  base::Value::Dict& dict = value.GetDict();

  static constexpr char kKeyApiConfig[] = "api_config";
  if (const base::Value* api_config = dict.Find(kKeyApiConfig)) {
    bool success = parser.ParseConfig(*api_config, config, /*required=*/false,
                                      kKeyApiConfig);
    is_config_valid &= success;
    EXPECT_TRUE(success) << error_stream.str();
    error_stream.str("");
  }

  absl::optional<base::Value> input =
      parser.SimulatorInputFromInteropInput(dict);
  EXPECT_TRUE(input) << error_stream.str();

  base::Value* expected_output = dict.Find("output");
  EXPECT_TRUE(expected_output);

  ASSERT_TRUE(is_config_valid && input);

  AttributionSimulationOptions options{
      .noise_mode = AttributionNoiseMode::kNone,
      .config = config,
      .delay_mode = AttributionDelayMode::kDefault,
      .output_options =
          AttributionSimulationOutputOptions{
              .remove_report_ids = true,
              .remove_assembled_report = true,
          },
  };

  base::Value simulator_output =
      RunAttributionSimulation(std::move(*input), options, error_stream);
  ASSERT_FALSE(simulator_output.is_none()) << error_stream.str();

  absl::optional<base::Value> actual_output =
      parser.InteropOutputFromSimulatorOutput(std::move(simulator_output));
  EXPECT_TRUE(actual_output) << error_stream.str();

  if (expected_output) {
    EXPECT_THAT(actual_output, Optional(base::test::IsJson(*expected_output)));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AttributionInteropTestInputs,
    AttributionInteropTest,
    ::testing::ValuesIn(GetInputs()),
    /*name_generator=*/
    [](const ::testing::TestParamInfo<base::FilePath>& info) {
      return info.param.RemoveFinalExtension().BaseName().MaybeAsASCII();
    });

}  // namespace

}  // namespace content

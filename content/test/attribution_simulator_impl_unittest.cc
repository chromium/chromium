// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/public/browser/attribution_reporting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

base::Value ReadJsonFromFile(const base::FilePath& path) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(path, &contents));
  return base::test::ParseJson(contents);
}

// Removes `.input.json` from the path.
base::FilePath RemoveInputPathExtension(const base::FilePath& path) {
  EXPECT_EQ(FILE_PATH_LITERAL(".json"), path.FinalExtension());
  EXPECT_EQ(FILE_PATH_LITERAL(".input"),
            path.RemoveFinalExtension().FinalExtension());
  return path.RemoveFinalExtension().RemoveFinalExtension();
}

std::vector<base::FilePath> GetInputPaths() {
  base::FilePath input_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &input_dir);
  input_dir = input_dir.AppendASCII(
      "content/test/data/attribution_reporting/simulator");

  std::vector<base::FilePath> input_paths;

  base::FileEnumerator e(input_dir, /*recursive=*/false,
                         base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.input.json"));
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    input_paths.push_back(std::move(name));
  }

  return input_paths;
}

base::FilePath OutputPath(const base::FilePath& input_path) {
  return RemoveInputPathExtension(input_path).AddExtensionASCII("output.json");
}

base::FilePath OptionsPath(const base::FilePath& input_path) {
  return RemoveInputPathExtension(input_path).AddExtensionASCII("options.json");
}

void ParseOptions(const base::Value& dict,
                  AttributionSimulationOptions& options) {
  if (const std::string* delay_mode = dict.FindStringKey("delay_mode")) {
    if (*delay_mode == "none") {
      options.delay_mode = AttributionDelayMode::kNone;
    } else {
      ASSERT_EQ(*delay_mode, "default")
          << "unknown delay mode: " << *delay_mode;
      options.delay_mode = AttributionDelayMode::kDefault;
    }
  }

  if (const std::string* noise_mode = dict.FindStringKey("noise_mode")) {
    if (*noise_mode == "none") {
      options.noise_mode = AttributionNoiseMode::kNone;
    } else {
      ASSERT_EQ(*noise_mode, "default")
          << "unknown noise mode: " << *noise_mode;
      options.noise_mode = AttributionNoiseMode::kDefault;
    }
  }
}

class AttributionSimulatorImplTest
    : public ::testing::TestWithParam<base::FilePath> {};

TEST_P(AttributionSimulatorImplTest, HasExpectedOutput) {
  const base::FilePath input_path = GetParam();
  base::Value input = ReadJsonFromFile(input_path);

  // Tests are nondeterministic if noise is used or report IDs are present.
  AttributionSimulationOptions options{
      .noise_mode = AttributionNoiseMode::kNone,
      .delay_mode = AttributionDelayMode::kDefault,
      .output_options =
          AttributionSimulationOutputOptions{
              .remove_report_ids = true,
              .remove_assembled_report = true,
          },
  };

  const base::FilePath options_path = OptionsPath(input_path);
  if (base::PathExists(options_path))
    ParseOptions(ReadJsonFromFile(options_path), options);

  const base::Value expected_output = ReadJsonFromFile(OutputPath(input_path));

  std::ostringstream error_stream;
  EXPECT_THAT(RunAttributionSimulation(std::move(input), options, error_stream),
              base::test::IsJson(expected_output));
  EXPECT_EQ(error_stream.str(), "");
}

INSTANTIATE_TEST_SUITE_P(
    AttributionSimulatorInputs,
    AttributionSimulatorImplTest,
    ::testing::ValuesIn(GetInputPaths()),
    /*name_generator=*/
    [](const ::testing::TestParamInfo<base::FilePath>& info) {
      return RemoveInputPathExtension(info.param).BaseName().MaybeAsASCII();
    });

}  // namespace
}  // namespace content

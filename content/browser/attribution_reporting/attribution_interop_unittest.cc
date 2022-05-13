// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "content/public/browser/attribution_reporting.h"
#include "content/public/test/attribution_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ::testing::Optional;

base::Value ReadJsonFromFile(const base::FilePath& path) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(path, &contents));
  return base::test::ParseJson(contents);
}

std::vector<base::FilePath> GetInputs() {
  base::FilePath input_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &input_dir);
  input_dir =
      input_dir.AppendASCII("content/test/data/attribution_reporting/interop");

  std::vector<base::FilePath> input_paths;

  base::FileEnumerator e(input_dir, /*recursive=*/false,
                         base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.json"));

  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
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

  base::Value value = ReadJsonFromFile(GetParam());
  base::Value::Dict& dict = value.GetDict();

  absl::optional<base::Value> input =
      parser.SimulatorInputFromInteropInput(dict);
  EXPECT_TRUE(input) << error_stream.str();

  base::Value* expected_output = dict.Find("output");
  EXPECT_TRUE(expected_output);

  ASSERT_TRUE(input);

  AttributionSimulationOptions options{
      .noise_mode = AttributionNoiseMode::kNone,
      .delay_mode = AttributionDelayMode::kDefault,
      .remove_report_ids = true,
      .report_time_format =
          AttributionReportTimeFormat::kMillisecondsSinceUnixEpoch,
      .remove_assembled_report = true,
  };

  base::Value simulator_output =
      RunAttributionSimulation(std::move(*input), options, error_stream);
  ASSERT_FALSE(simulator_output.is_none()) << error_stream.str();

  absl::optional<base::Value> actual_output =
      parser.InteropOutputFromSimulatorOutput(std::move(simulator_output));
  EXPECT_TRUE(actual_output) << error_stream.str();

  if (expected_output)
    EXPECT_THAT(actual_output, Optional(base::test::IsJson(*expected_output)));
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

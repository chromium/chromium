// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
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

class AttributionSimulatorImplTest
    : public ::testing::TestWithParam<base::FilePath> {};

TEST_P(AttributionSimulatorImplTest, HasExpectedOutput) {
  const base::FilePath input_path = GetParam();
  base::Value input = ReadJsonFromFile(input_path);

  const base::Value expected_output = ReadJsonFromFile(OutputPath(input_path));

  base::Value output = RunAttributionSimulationOrExit(
      std::move(input), AttributionSimulationOptions{
                            .noise_mode = AttributionNoiseMode::kNone,
                            .delay_mode = AttributionDelayMode::kDefault,
                            .remove_report_ids = true,
                        });

  EXPECT_THAT(output, base::test::IsJson(expected_output));
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

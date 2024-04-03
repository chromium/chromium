// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_interop_parser.h"
#include "content/browser/attribution_reporting/attribution_interop_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::UnorderedElementsAreArray;

constexpr char kDefaultConfigFileName[] = "default_config.json";

base::FilePath GetInputDir() {
  base::FilePath input_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &input_dir);
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

void PreProcessOutput(AttributionInteropOutput& output) {
  // Ensure that integral values for this field are replaced with the equivalent
  // double, since they are equivalent at the JSON level.
  for (auto& report : output.reports) {
    base::Value::Dict* dict = report.payload.GetIfDict();
    if (!dict) {
      continue;
    }

    base::Value* rate = dict->Find("randomized_trigger_rate");
    if (!rate || !rate->is_int()) {
      continue;
    }

    // This coerces the integer to a double.
    *rate = base::Value(rate->GetDouble());
  }
}

class AttributionInteropTest : public ::testing::TestWithParam<base::FilePath> {
 public:
  static void SetUpTestSuite() {
    ASSERT_OK_AND_ASSIGN(
        g_config_,
        ParseAttributionInteropConfig(base::test::ParseJsonDictFromFile(
            GetInputDir().AppendASCII(kDefaultConfigFileName))));
  }

 protected:
  static AttributionInteropConfig GetConfig() { return g_config_; }

 private:
  static AttributionInteropConfig g_config_;
};

// static
AttributionInteropConfig AttributionInteropTest::g_config_;

// See //content/test/data/attribution_reporting/interop/README.md for the
// JSON schema.
TEST_P(AttributionInteropTest, HasExpectedOutput) {
  AttributionInteropConfig config = GetConfig();
  base::Value::Dict dict = base::test::ParseJsonDictFromFile(GetParam());

  if (const base::Value* api_config = dict.Find("api_config")) {
    ASSERT_TRUE(api_config->is_dict());
    ASSERT_THAT(MergeAttributionInteropConfig(api_config->GetDict(), config),
                base::test::HasValue());
  }

  std::optional<base::Value> input = dict.Extract("input");
  ASSERT_TRUE(input && input->is_dict());

  std::optional<base::Value> output = dict.Extract("output");
  ASSERT_TRUE(output && output->is_dict());

  ASSERT_OK_AND_ASSIGN(
      auto expected_output,
      AttributionInteropOutput::Parse(std::move(*output).TakeDict()));

  ASSERT_OK_AND_ASSIGN(
      AttributionInteropOutput actual_output,
      RunAttributionInteropSimulation(std::move(*input).TakeDict(), config));

  PreProcessOutput(expected_output);
  PreProcessOutput(actual_output);

  EXPECT_THAT(actual_output,
              Field(&AttributionInteropOutput::reports,
                    UnorderedElementsAreArray(expected_output.reports)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AttributionInteropTest,
    ::testing::ValuesIn(GetInputs()),
    /*name_generator=*/
    [](const ::testing::TestParamInfo<base::FilePath>& info) {
      return info.param.RemoveFinalExtension().BaseName().MaybeAsASCII();
    });

}  // namespace

}  // namespace content

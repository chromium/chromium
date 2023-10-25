// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/features.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_interop_parser.h"
#include "content/browser/attribution_reporting/attribution_interop_runner.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
        g_config_, ParseAttributionConfig(base::test::ParseJsonDictFromFile(
                       GetInputDir().AppendASCII(kDefaultConfigFileName))));
  }

  AttributionInteropTest() {
    // This UMA records a sample every 30s via a periodic task which
    // interacts poorly with TaskEnvironment::FastForward using day long
    // delays (we need to run the uma update every 30s for that
    // interval)
    scoped_feature_list_.InitAndDisableFeature(
        network::features::kGetCookiesStringUma);
  }

 protected:
  static AttributionConfig GetConfig() { return g_config_; }

 private:
  static AttributionConfig g_config_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// static
AttributionConfig AttributionInteropTest::g_config_;

// See //content/test/data/attribution_reporting/interop/README.md for the
// JSON schema.
TEST_P(AttributionInteropTest, HasExpectedOutput) {
  AttributionConfig config = GetConfig();
  base::Value::Dict dict = base::test::ParseJsonDictFromFile(GetParam());

  base::test::ScopedFeatureList scoped_feature_list;
  if (dict.FindBool("needs_trigger_config").value_or(false)) {
    scoped_feature_list.InitAndEnableFeature(
        attribution_reporting::features::kAttributionReportingTriggerConfig);
  }

  if (const base::Value* api_config = dict.Find("api_config")) {
    ASSERT_TRUE(api_config->is_dict());
    ASSERT_EQ("", MergeAttributionConfig(api_config->GetDict(), config));
  }

  absl::optional<base::Value> input = dict.Extract("input");
  ASSERT_TRUE(input && input->is_dict());

  absl::optional<base::Value> output = dict.Extract("output");
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
              AllOf(Field(&AttributionInteropOutput::reports,
                          UnorderedElementsAreArray(expected_output.reports)),
                    Field(&AttributionInteropOutput::unparsable_registrations,
                          UnorderedElementsAreArray(
                              expected_output.unparsable_registrations))));
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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_interop_parser.h"
#include "content/browser/attribution_reporting/attribution_interop_runner.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr char kDefaultConfigFileName[] = "default_config.json";

base::Value::Dict ReadJsonFromFile(const base::FilePath& path) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(path, &contents));
  return base::test::ParseJsonDict(contents);
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

void ProcessReports(base::Value::Dict& dict, base::StringPiece key) {
  base::Value::List* list = dict.FindList(key);
  if (!list) {
    return;
  }
  if (list->empty()) {
    dict.Remove(key);
    return;
  }
  base::ranges::sort(*list);
}

class AttributionInteropTest : public ::testing::TestWithParam<base::FilePath> {
 public:
  static void SetUpTestSuite() {
    ASSERT_OK_AND_ASSIGN(
        g_config_, ParseAttributionConfig(ReadJsonFromFile(
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
  base::Value::Dict dict = ReadJsonFromFile(GetParam());

  if (const base::Value* api_config = dict.Find("api_config")) {
    ASSERT_TRUE(api_config->is_dict());
    ASSERT_EQ("", MergeAttributionConfig(api_config->GetDict(), config));
  }

  absl::optional<base::Value> input = dict.Extract("input");
  ASSERT_TRUE(input && input->is_dict());

  ASSERT_OK_AND_ASSIGN(
      auto actual_output,
      RunAttributionInteropSimulation(std::move(*input).TakeDict(), config));

  absl::optional<base::Value> expected_output = dict.Extract("output");
  ASSERT_TRUE(expected_output.has_value());

  base::Value::Dict& expected_output_dict = expected_output->GetDict();

  for (const char* key : {kEventLevelResultsKey, kDebugEventLevelResultsKey,
                          kAggregatableResultsKey, kDebugAggregatableResultsKey,
                          kVerboseDebugReportsKey}) {
    ProcessReports(actual_output, key);
    ProcessReports(expected_output_dict, key);
  }

  EXPECT_THAT(actual_output, base::test::IsJson(expected_output_dict));
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

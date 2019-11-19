// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"

#include <functional>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "components/crx_file/id_util.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

api::declarative_net_request::Rule GetAPIRule(const TestRule& rule) {
  std::unique_ptr<base::DictionaryValue> value = rule.ToValue();
  EXPECT_TRUE(value);
  api::declarative_net_request::Rule result;
  base::string16 error;
  EXPECT_TRUE(
      api::declarative_net_request::Rule::Populate(*value, &result, &error))
      << error;
  EXPECT_TRUE(error.empty()) << error;
  return result;
}

struct LoadRulesetResult {
  bool has_new_checksum = false;
  base::Optional<bool> reindexing_successful;
  RulesetMatcher::LoadRulesetResult load_result =
      RulesetMatcher::kLoadResultMax;
};

struct TestCase {
  explicit TestCase(RulesetSource source) : source(std::move(source)) {}
  int checksum;
  RulesetSource source;
  LoadRulesetResult expected_result;
};

class FileSequenceHelperTest : public ExtensionsTest {
 public:
  FileSequenceHelperTest() : channel_(::version_info::Channel::UNKNOWN) {}

  // ExtensonsTest overrides:
  void SetUp() override {
    ExtensionsTest::SetUp();
    helper_ = std::make_unique<FileSequenceHelper>();
  }
  void TearDown() override {
    GetExtensionFileTaskRunner()->DeleteSoon(FROM_HERE, std::move(helper_));
    base::RunLoop().RunUntilIdle();
    ExtensionsTest::TearDown();
  }

  void TestAddDynamicRules(
      RulesetSource source,
      std::vector<api::declarative_net_request::Rule> rules_to_add,
      ReadJSONRulesResult::Status expected_read_status,
      UpdateDynamicRulesStatus expected_update_status,
      base::Optional<std::string> expected_error,
      bool expected_did_load_successfully) {
    base::RunLoop run_loop;
    auto add_rules_callback = base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_did_load_successfully,
           base::Optional<std::string> expected_error, LoadRequestData data,
           base::Optional<std::string> error) {
          EXPECT_EQ(1u, data.rulesets.size());
          EXPECT_EQ(expected_did_load_successfully,
                    data.rulesets[0].did_load_successfully());
          EXPECT_EQ(expected_error, error) << error.value_or("no actual error");
          run_loop->Quit();
        },
        &run_loop, expected_did_load_successfully, expected_error);

    ExtensionId extension_id = crx_file::id_util::GenerateId("dummy_extension");
    LoadRequestData data(extension_id);
    data.rulesets.emplace_back(std::move(source));

    // Unretained is safe because |helper_| outlives the |add_rules_task|.
    auto add_rules_task =
        base::BindOnce(&FileSequenceHelper::UpdateDynamicRules,
                       base::Unretained(helper_.get()), std::move(data),
                       /* rule_ids_to_remove */ std::vector<int>(),
                       std::move(rules_to_add), std::move(add_rules_callback));

    base::HistogramTester tester;
    GetExtensionFileTaskRunner()->PostTask(FROM_HERE,
                                           std::move(add_rules_task));
    run_loop.Run();
    tester.ExpectUniqueSample(kUpdateDynamicRulesStatusHistogram,
                              expected_update_status, 1 /* expected_count */);
    tester.ExpectUniqueSample(kReadDynamicRulesJSONStatusHistogram,
                              expected_read_status, 1 /* expected_count */);
  }

  void TestLoadRulesets(const std::vector<TestCase>& test_cases) {
    ExtensionId extension_id = crx_file::id_util::GenerateId("dummy_extension");

    LoadRequestData data(extension_id);
    for (const auto& test_case : test_cases) {
      data.rulesets.emplace_back(test_case.source.Clone());
      data.rulesets.back().set_expected_checksum(test_case.checksum);
    }

    base::RunLoop run_loop;
    auto load_ruleset_callback = base::BindOnce(
        [](base::RunLoop* run_loop, const std::vector<TestCase>& test_cases,
           LoadRequestData data) {
          // Verify |data| is as expected.
          ASSERT_EQ(data.rulesets.size(), test_cases.size());

          for (size_t i = 0; i < data.rulesets.size(); i++) {
            const RulesetInfo& ruleset = data.rulesets[i];
            const LoadRulesetResult& expected_result =
                test_cases[i].expected_result;

            EXPECT_EQ(expected_result.has_new_checksum,
                      ruleset.new_checksum().has_value());
            EXPECT_EQ(expected_result.reindexing_successful,
                      ruleset.reindexing_successful());
            EXPECT_EQ(expected_result.load_result,
                      ruleset.load_ruleset_result());
          }

          run_loop->Quit();
        },
        &run_loop, std::cref(test_cases));

    // Unretained is safe because |helper_| outlives the |load_ruleset_task|.
    auto load_ruleset_task = base::BindOnce(
        &FileSequenceHelper::LoadRulesets, base::Unretained(helper_.get()),
        std::move(data), std::move(load_ruleset_callback));
    GetExtensionFileTaskRunner()->PostTask(FROM_HERE,
                                           std::move(load_ruleset_task));
    run_loop.Run();
  }

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  std::unique_ptr<FileSequenceHelper> helper_;

  // Required to use DataDecoder's JSON parsing for re-indexing.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceHelperTest);
};

// Tests loading and reindexing multiple rulesets.
TEST_F(FileSequenceHelperTest, MultipleRulesets) {
  const int kNumRulesets = 3;
  std::vector<TestCase> test_cases;

  // First create |kNumRulesets| indexed rulesets.
  for (size_t i = 0; i < kNumRulesets; i++) {
    test_cases.emplace_back(CreateTemporarySource());

    auto& test_case = test_cases.back();

    std::unique_ptr<RulesetMatcher> matcher;
    ASSERT_TRUE(CreateVerifiedMatcher({CreateGenericRule()}, test_case.source,
                                      &matcher, &test_case.checksum));

    // Initially loading all the rulesets should succeed.
    test_case.expected_result.load_result = RulesetMatcher::kLoadSuccess;
  }

  TestLoadRulesets(test_cases);

  // Now delete the first and third indexed rulesets. This would cause a
  // re-index.
  base::DeleteFile(test_cases[0].source.indexed_path(), false /* recursive */);
  base::DeleteFile(test_cases[2].source.indexed_path(), false /* recursive */);
  test_cases[0].expected_result.reindexing_successful = true;
  test_cases[2].expected_result.reindexing_successful = true;

  TestLoadRulesets(test_cases);

  // The files should have been re-indexed.
  EXPECT_TRUE(base::PathExists(test_cases[0].source.indexed_path()));
  EXPECT_TRUE(base::PathExists(test_cases[2].source.indexed_path()));

  // Reset state.
  test_cases[0].expected_result.reindexing_successful = base::nullopt;
  test_cases[2].expected_result.reindexing_successful = base::nullopt;

  // Change the expected checksum for rulesets 2 and 3. Loading both of the
  // rulesets should now fail due to a checksum mismatch.
  test_cases[1].checksum--;
  test_cases[2].checksum--;
  test_cases[1].expected_result.load_result =
      RulesetMatcher::kLoadErrorChecksumMismatch;
  test_cases[2].expected_result.load_result =
      RulesetMatcher::kLoadErrorChecksumMismatch;
  test_cases[1].expected_result.reindexing_successful = false;
  test_cases[2].expected_result.reindexing_successful = false;

  TestLoadRulesets(test_cases);

  // Reset checksums.
  test_cases[1].checksum++;
  test_cases[2].checksum++;

  // Now simulate a flatbuffer version mismatch.
  const int kIndexedRulesetFormatVersion = 100;
  std::string old_version_header = GetVersionHeaderForTesting();
  SetIndexedRulesetFormatVersionForTesting(kIndexedRulesetFormatVersion);
  ASSERT_NE(old_version_header, GetVersionHeaderForTesting());

  // Version mismatch will cause reindexing and updated checksums.
  for (auto& test_case : test_cases) {
    test_case.expected_result.reindexing_successful = true;
    test_case.expected_result.has_new_checksum = true;
    test_case.expected_result.load_result = RulesetMatcher::kLoadSuccess;
  }

  TestLoadRulesets(test_cases);
}

// Tests updating dynamic rules.
TEST_F(FileSequenceHelperTest, UpdateDynamicRules) {
  // Simulate adding rules for the first time i.e. with no JSON and indexed
  // ruleset files.
  RulesetSource source = CreateTemporarySource();
  base::DeleteFile(source.json_path(), false /* recursive */);
  base::DeleteFile(source.indexed_path(), false /* recursive */);

  // Test success.
  std::vector<api::declarative_net_request::Rule> api_rules;
  {
    SCOPED_TRACE("Test adding a valid rule");
    api_rules.push_back(GetAPIRule(CreateGenericRule()));
    TestAddDynamicRules(source.Clone(), std::move(api_rules),
                        ReadJSONRulesResult::Status::kFileDoesNotExist,
                        UpdateDynamicRulesStatus::kSuccess,
                        base::nullopt /* expected_error */,
                        true /* expected_did_load_successfully*/);
  }

  // Test adding an invalid rule, e.g. a redirect rule without priority.
  {
    SCOPED_TRACE("Test adding an invalid rule");
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID + 1;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->url = std::string("http://google.com");
    api_rules.clear();
    api_rules.push_back(GetAPIRule(rule));
    TestAddDynamicRules(
        source.Clone(), std::move(api_rules),
        ReadJSONRulesResult::Status::kSuccess,
        UpdateDynamicRulesStatus::kErrorInvalidRules,
        ParseInfo(ParseResult::ERROR_EMPTY_REDIRECT_RULE_PRIORITY,
                  kMinValidID + 1)
            .GetErrorDescription(),
        false /* expected_did_load_successfully */);
  }

  // Write invalid JSON to the JSON rules file. The update should still succeed.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_testing;
    std::string data = "Invalid JSON";
    ASSERT_EQ(data.size(), static_cast<size_t>(base::WriteFile(
                               source.json_path(), data.c_str(), data.size())));
  }

  {
    SCOPED_TRACE("Test corrupted JSON rules file");
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID + 2;
    api_rules.clear();
    api_rules.push_back(GetAPIRule(rule));
    TestAddDynamicRules(source.Clone(), std::move(api_rules),
                        ReadJSONRulesResult::Status::kJSONParseError,
                        UpdateDynamicRulesStatus::kSuccess,
                        base::nullopt /* expected_error */,
                        true /* expected_did_load_successfully*/);
  }
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions

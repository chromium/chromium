// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"

#include <functional>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/rule_counts.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::declarative_net_request {
namespace {

api::declarative_net_request::Rule GetAPIRule(const TestRule& rule) {
  std::u16string error;
  auto result = api::declarative_net_request::Rule::FromValue(rule.ToValue());
  EXPECT_TRUE(result.has_value()) << result.error();
  return std::move(result).value_or(api::declarative_net_request::Rule());
}

struct TestLoadRulesetInfo {
  bool has_new_checksum = false;
  std::optional<bool> indexing_successful;
  std::optional<LoadRulesetResult> load_result;
};

struct TestCase {
  explicit TestCase(FileBackedRulesetSource source)
      : source(std::move(source)) {}
  int checksum;
  FileBackedRulesetSource source;
  TestLoadRulesetInfo expected_result;
};

ExtensionId GenerateDummyExtensionID() {
  return crx_file::id_util::GenerateId("dummy_extension");
}

class FileSequenceHelperTest : public ExtensionsTest {
 public:
  FileSequenceHelperTest() = default;

  FileSequenceHelperTest(const FileSequenceHelperTest&) = delete;
  FileSequenceHelperTest& operator=(const FileSequenceHelperTest&) = delete;

  // ExtensionsTest overrides:
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
      FileBackedRulesetSource source,
      std::vector<api::declarative_net_request::Rule> rules_to_add,
      ReadJSONRulesResult::Status expected_read_status,
      UpdateDynamicRulesStatus expected_update_status,
      std::optional<std::string> expected_error,
      bool expected_did_load_successfully) {
    base::RunLoop run_loop;
    auto add_rules_callback = base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_did_load_successfully,
           std::optional<std::string> expected_error, LoadRequestData data,
           std::optional<std::string> error) {
          EXPECT_EQ(1u, data.rulesets.size());
          EXPECT_EQ(expected_error, error) << error.value_or("no actual error");
          EXPECT_EQ(expected_did_load_successfully,
                    data.rulesets[0].did_load_successfully());
          run_loop->Quit();
        },
        &run_loop, expected_did_load_successfully, expected_error);

    ExtensionId extension_id = crx_file::id_util::GenerateId("dummy_extension");
    LoadRequestData data(extension_id, base::Version("1.0"));
    data.rulesets.emplace_back(std::move(source));

    // Unretained is safe because |helper_| outlives the |add_rules_task|.
    auto add_rules_task = base::BindOnce(
        &FileSequenceHelper::UpdateDynamicRules,
        base::Unretained(helper_.get()), std::move(data),
        /* rule_ids_to_remove */ std::vector<int>(), std::move(rules_to_add),
        RuleCounts(GetDynamicRuleLimit(), GetUnsafeDynamicRuleLimit(),
                   GetRegexRuleLimit()),
        std::move(add_rules_callback));

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
    LoadRequestData data(GenerateDummyExtensionID(), base::Version("1.0"));
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
            SCOPED_TRACE(base::StringPrintf("Testing ruleset %" PRIuS, i));
            const RulesetInfo& ruleset = data.rulesets[i];
            const TestLoadRulesetInfo& expected_result =
                test_cases[i].expected_result;

            EXPECT_EQ(expected_result.has_new_checksum,
                      ruleset.new_checksum().has_value());
            EXPECT_EQ(expected_result.indexing_successful,
                      ruleset.indexing_successful());
            ASSERT_TRUE(ruleset.load_ruleset_result());
            EXPECT_EQ(expected_result.load_result,
                      ruleset.load_ruleset_result())
                << *ruleset.load_ruleset_result();
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

  void TestNoRulesetsToLoad() {
    LoadRequestData data(GenerateDummyExtensionID(), base::Version("1.0"));

    base::RunLoop run_loop;
    auto load_ruleset_callback = base::BindOnce(
        [](base::RunLoop* run_loop, LoadRequestData data) { run_loop->Quit(); },
        &run_loop);

    auto load_ruleset_task = base::BindOnce(
        &FileSequenceHelper::LoadRulesets, base::Unretained(helper_.get()),
        std::move(data), std::move(load_ruleset_callback));
    GetExtensionFileTaskRunner()->PostTask(FROM_HERE,
                                           std::move(load_ruleset_task));
    run_loop.Run();
  }

  // Initialize |num_rulesets| rulesets and returns the corresponding test
  // cases.
  std::vector<TestCase> InitializeRulesets(size_t num_rulesets) const {
    std::vector<TestCase> test_cases;
    test_cases.reserve(num_rulesets);

    for (size_t i = 0; i < num_rulesets; i++) {
      test_cases.emplace_back(CreateTemporarySource());

      auto& test_case = test_cases.back();

      std::unique_ptr<RulesetMatcher> matcher;
      EXPECT_TRUE(CreateVerifiedMatcher({CreateGenericRule()}, test_case.source,
                                        &matcher, &test_case.checksum));

      // Initially loading all the rulesets should succeed.
      test_case.expected_result.load_result = LoadRulesetResult::kSuccess;
    }
    return test_cases;
  }

 private:
  std::unique_ptr<FileSequenceHelper> helper_;

  // Required to use DataDecoder's JSON parsing for re-indexing.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(FileSequenceHelperTest, NoRulesetsToLoad) {
  TestNoRulesetsToLoad();
}

TEST_F(FileSequenceHelperTest, IndexedRulesetDeleted) {
  const size_t kNumRulesets = 3;
  std::vector<TestCase> test_cases = InitializeRulesets(kNumRulesets);

  TestLoadRulesets(test_cases);

  // Now delete the first and third indexed rulesets. This would cause a
  // re-index.
  base::DeleteFile(test_cases[0].source.indexed_path());
  base::DeleteFile(test_cases[2].source.indexed_path());
  test_cases[0].expected_result.indexing_successful = true;
  test_cases[2].expected_result.indexing_successful = true;

  TestLoadRulesets(test_cases);

  // The files should have been re-indexed.
  EXPECT_TRUE(base::PathExists(test_cases[0].source.indexed_path()));
  EXPECT_TRUE(base::PathExists(test_cases[2].source.indexed_path()));
}

TEST_F(FileSequenceHelperTest, ChecksumMismatch) {
  const size_t kNumRulesets = 4;
  std::vector<TestCase> test_cases = InitializeRulesets(kNumRulesets);

  TestLoadRulesets(test_cases);

  // Change the expected checksum for rulesets 2 and 3. Loading both of the
  // rulesets should now fail due to a checksum mismatch.
  test_cases[1].checksum--;
  test_cases[2].checksum--;
  test_cases[1].expected_result.load_result =
      LoadRulesetResult::kErrorChecksumMismatch;
  test_cases[2].expected_result.load_result =
      LoadRulesetResult::kErrorChecksumMismatch;
  test_cases[1].expected_result.indexing_successful = false;
  test_cases[2].expected_result.indexing_successful = false;

  TestLoadRulesets(test_cases);
}

TEST_F(FileSequenceHelperTest, RulesetFormatVersionMismatch) {
  const size_t kNumRulesets = 4;
  std::vector<TestCase> test_cases = InitializeRulesets(kNumRulesets);

  TestLoadRulesets(test_cases);

  // Now simulate a flatbuffer version mismatch.
  ScopedIncrementRulesetVersion scoped_version_change =
      CreateScopedIncrementRulesetVersionForTesting();

  // Version mismatch will cause re-indexing and updated checksums.
  for (auto& test_case : test_cases) {
    test_case.expected_result.indexing_successful = true;
    test_case.expected_result.has_new_checksum = true;
    test_case.expected_result.load_result = LoadRulesetResult::kSuccess;
  }

  TestLoadRulesets(test_cases);
}

TEST_F(FileSequenceHelperTest, JSONAndIndexedRulesetDeleted) {
  const size_t kNumRulesets = 3;
  std::vector<TestCase> test_cases = InitializeRulesets(kNumRulesets);

  TestLoadRulesets(test_cases);

  base::DeleteFile(test_cases[0].source.json_path());
  base::DeleteFile(test_cases[1].source.json_path());
  base::DeleteFile(test_cases[0].source.indexed_path());
  base::DeleteFile(test_cases[1].source.indexed_path());

  // Re-indexing will fail since the JSON ruleset is now deleted.
  test_cases[0].expected_result.indexing_successful = false;
  test_cases[1].expected_result.indexing_successful = false;

  test_cases[0].expected_result.load_result =
      LoadRulesetResult::kErrorInvalidPath;
  test_cases[1].expected_result.load_result =
      LoadRulesetResult::kErrorInvalidPath;
  test_cases[2].expected_result.load_result = LoadRulesetResult::kSuccess;

  TestLoadRulesets(test_cases);
}

// Tests updating dynamic rules.
TEST_F(FileSequenceHelperTest, UpdateDynamicRules) {
  // Simulate adding rules for the first time i.e. with no JSON and indexed
  // ruleset files.
  FileBackedRulesetSource source = CreateTemporarySource();
  base::DeleteFile(source.json_path());
  base::DeleteFile(source.indexed_path());

  // Test success.
  std::vector<api::declarative_net_request::Rule> api_rules;
  {
    SCOPED_TRACE("Test adding a valid rule");
    api_rules.push_back(GetAPIRule(CreateGenericRule()));
    TestAddDynamicRules(source.Clone(), std::move(api_rules),
                        ReadJSONRulesResult::Status::kFileDoesNotExist,
                        UpdateDynamicRulesStatus::kSuccess,
                        std::nullopt /* expected_error */,
                        true /* expected_did_load_successfully*/);
  }

  // Test adding an invalid rule, e.g. a rule with invalid priority.
  {
    SCOPED_TRACE("Test adding an invalid rule");
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID + 1;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->url = std::string("http://google.com");
    rule.priority = kMinValidPriority - 1;
    api_rules.clear();
    api_rules.push_back(GetAPIRule(rule));

    int rule_id = kMinValidID + 1;
    ParseInfo info(ParseResult::ERROR_INVALID_RULE_PRIORITY, rule_id);
    TestAddDynamicRules(source.Clone(), std::move(api_rules),
                        ReadJSONRulesResult::Status::kSuccess,
                        UpdateDynamicRulesStatus::kErrorInvalidRules,
                        info.error(),
                        false /* expected_did_load_successfully */);
  }

  // Write invalid JSON to the JSON rules file. The update should still succeed.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_testing;
    std::string data = "Invalid JSON";
    ASSERT_TRUE(base::WriteFile(source.json_path(), data));
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
                        std::nullopt /* expected_error */,
                        true /* expected_did_load_successfully*/);
  }
}

}  // namespace
}  // namespace extensions::declarative_net_request

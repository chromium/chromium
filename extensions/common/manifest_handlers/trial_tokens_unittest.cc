// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/trial_tokens_handler.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TrialTokensManifestTest : public ManifestTest {
 public:
  TrialTokensManifestTest() {}

 protected:
  // TODO(crbug.com/40282364): Add validation of trial token contents
  // in TrialTokensHandler::Parse() and generate proper (signed) tokens
  // to use here
  // See this for a reference:
  // third_party/blink/public/common/origin_trials/trial_token_validator.cc
  static constexpr char kTestExtensionID[] = "id";
  static constexpr char kValidToken1[] = "valid_token_1";
  static constexpr char kValidToken2[] = "valid_token_2";
  static constexpr char kValidToken3[] = "valid_token_3";

  base::Value::Dict CreateManifest(const std::string& trial_tokens) {
    return base::Value::Dict()
        .Set("name", "test")
        .Set("version", "1")
        .Set("manifest_version", 3)
        .Set("trial_tokens", base::test::ParseJson(trial_tokens));
  }

  base::Value::Dict CreateManifestNoTrialTokens() {
    return base::Value::Dict()
        .Set("name", "test")
        .Set("version", "1")
        .Set("manifest_version", 3);
  }

  scoped_refptr<Extension> LoadExtension(
      const std::string& manifest_trial_tokens) {
    std::string error;
    base::FilePath test_data_dir = GetTestDataDir();
    scoped_refptr<Extension> extension = Extension::Create(
        test_data_dir.DirName(), mojom::ManifestLocation::kInternal,
        CreateManifest(manifest_trial_tokens), extensions::Extension::NO_FLAGS,
        kTestExtensionID, &error);

    // 'trial_tokens' never results in installation failure
    EXPECT_TRUE(error.empty());

    return extension;
  }

  void RunTestCase(const std::string& manifest_trial_tokens,
                   const std::set<std::string>* expected_trial_tokens,
                   const std::vector<std::string>& expected_warnings) {
    scoped_refptr<Extension> extension = LoadExtension(manifest_trial_tokens);

    const std::set<std::string>* trial_tokens =
        TrialTokens::GetTrialTokens(*extension.get());

    EXPECT_EQ(trial_tokens == nullptr,
              !TrialTokens::HasTrialTokens(*extension.get()));

    if (expected_trial_tokens) {
      ASSERT_TRUE(trial_tokens);
      EXPECT_EQ(*expected_trial_tokens, *trial_tokens);
    } else {
      EXPECT_EQ(nullptr, trial_tokens);
    }

    std::vector<InstallWarning> expected_install_warnings;
    expected_install_warnings.reserve(expected_warnings.size());
    for (const std::string& warning : expected_warnings) {
      expected_install_warnings.push_back(InstallWarning(warning));
    }
    EXPECT_EQ(expected_install_warnings, extension.get()->install_warnings());
  }

  void RunTestCase(const std::string& manifest_trial_tokens,
                   const std::set<std::string>& expected_trial_tokens,
                   const std::vector<std::string>& expected_warnings) {
    RunTestCase(manifest_trial_tokens, &expected_trial_tokens,
                expected_warnings);
  }
};

}  // namespace

TEST_F(TrialTokensManifestTest, InvalidTrialTokensList) {
  // Must be a non-empty list
  RunTestCase("32", nullptr,
              {manifest_errors::kInvalidTrialTokensNonEmptyList});
  RunTestCase("true", nullptr,
              {manifest_errors::kInvalidTrialTokensNonEmptyList});
  RunTestCase(R"("not_a_valid_token_list")", nullptr,
              {manifest_errors::kInvalidTrialTokensNonEmptyList});
  RunTestCase("{}", nullptr,
              {manifest_errors::kInvalidTrialTokensNonEmptyList});
  RunTestCase(R"({"foo": false})", nullptr,
              {manifest_errors::kInvalidTrialTokensNonEmptyList});
  RunTestCase(R"([])", nullptr,
              {manifest_errors::kInvalidTrialTokensNonEmptyList});
}

TEST_F(TrialTokensManifestTest, InvalidTrialTokensValue) {
  // Every token must be a non-empty string
  RunTestCase(R"([""])", nullptr, {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(R"([32])", nullptr, {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(R"([true])", nullptr,
              {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(base::StringPrintf(R"([["%s"]])", kValidToken1), nullptr,
              {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(base::StringPrintf(R"(["%s", ""])", kValidToken1), {kValidToken1},
              {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(base::StringPrintf(R"(["%s", 32])", kValidToken1), {kValidToken1},
              {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(base::StringPrintf(R"(["%s", true])", kValidToken1),
              {kValidToken1}, {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(
      base::StringPrintf(R"(["%s", ["%s"]])", kValidToken1, kValidToken1),
      {kValidToken1}, {manifest_errors::kInvalidTrialTokensValue});
  RunTestCase(
      base::StringPrintf(R"(["%s", "%s"])", kValidToken1, kValidToken1),
      {kValidToken1},
      {base::StringPrintf(manifest_errors::kInvalidTrialTokensValueDuplicate,
                          kValidToken1)});
  RunTestCase(
      base::StringPrintf(R"(["%s", "%s", "%s", "%s", "%s"])", kValidToken1,
                         kValidToken2, kValidToken2, kValidToken3,
                         kValidToken1),
      {kValidToken1, kValidToken2, kValidToken3},
      {base::StringPrintf(manifest_errors::kInvalidTrialTokensValueDuplicate,
                          kValidToken2),
       base::StringPrintf(manifest_errors::kInvalidTrialTokensValueDuplicate,
                          kValidToken1)});
}

TEST_F(TrialTokensManifestTest, NoTrialTokens) {
  scoped_refptr<Extension> good =
      LoadAndExpectSuccess(ManifestData(CreateManifestNoTrialTokens()));
  EXPECT_FALSE(TrialTokens::HasTrialTokens(*good.get()));
  EXPECT_EQ(TrialTokens::GetTrialTokens(*good.get()), nullptr);
}

TEST_F(TrialTokensManifestTest, VerifyParse) {
  scoped_refptr<Extension> good = LoadAndExpectSuccess(ManifestData(
      CreateManifest(base::StringPrintf(R"(["%s"])", kValidToken1))));
  EXPECT_TRUE(TrialTokens::HasTrialTokens(*good.get()));

  const std::set<std::string>* tokens =
      TrialTokens::GetTrialTokens(*good.get());
  ASSERT_TRUE(tokens);
  EXPECT_EQ(std::set({std::string(kValidToken1)}), *tokens);
}

}  // namespace extensions

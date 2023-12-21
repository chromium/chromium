// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "components/version_info/version_info.h"
#include "extensions/common/features/feature_channel.h"
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
  ManifestData CreateManifest(const std::string& trial_tokens) {
    static constexpr char kManifest[] =
        R"({
             "name": "test",
             "version": "1",
             "manifest_version": 3,
             "trial_tokens": %s
           })";
    base::Value manifest = base::test::ParseJson(
        base::StringPrintf(kManifest, trial_tokens.c_str()));
    return ManifestData(std::move(manifest).TakeDict());
  }

  ManifestData CreateManifestNoTrialTokens() {
    base::Value manifest = base::test::ParseJson(R"({
             "name": "test",
             "version": "1",
             "manifest_version": 3
           })");
    return ManifestData(std::move(manifest).TakeDict());
  }

 private:
  ScopedCurrentChannel channel_{version_info::Channel::CANARY};
};

}  // namespace

TEST_F(TrialTokensManifestTest, InvalidTrialTokensList) {
  // Must be a non-empty list
  LoadAndExpectError(CreateManifest("32"),
                     manifest_errors::kInvalidTrialTokensNonEmptyList);
  LoadAndExpectError(CreateManifest("true"),
                     manifest_errors::kInvalidTrialTokensNonEmptyList);
  LoadAndExpectError(CreateManifest(R"("not_a_valid_token_list")"),
                     manifest_errors::kInvalidTrialTokensNonEmptyList);
  LoadAndExpectError(CreateManifest("{}"),
                     manifest_errors::kInvalidTrialTokensNonEmptyList);
  LoadAndExpectError(CreateManifest(R"({"foo": false})"),
                     manifest_errors::kInvalidTrialTokensNonEmptyList);
  LoadAndExpectError(CreateManifest(R"([])"),
                     manifest_errors::kInvalidTrialTokensNonEmptyList);
}

TEST_F(TrialTokensManifestTest, InvalidTrialTokensValue) {
  // Every token must be a non-empty string
  LoadAndExpectError(CreateManifest(R"([""])"),
                     manifest_errors::kInvalidTrialTokensValue);
  LoadAndExpectError(CreateManifest(R"([32])"),
                     manifest_errors::kInvalidTrialTokensValue);
  LoadAndExpectError(CreateManifest(R"([true])"),
                     manifest_errors::kInvalidTrialTokensValue);
  LoadAndExpectError(CreateManifest(R"([["valid_token"]])"),
                     manifest_errors::kInvalidTrialTokensValue);
  LoadAndExpectError(CreateManifest(R"(["valid_token", ""])"),
                     manifest_errors::kInvalidTrialTokensValue);
  LoadAndExpectError(CreateManifest(R"(["valid_token", 32])"),
                     manifest_errors::kInvalidTrialTokensValue);
  LoadAndExpectError(CreateManifest(R"(["valid_token", true])"),
                     manifest_errors::kInvalidTrialTokensValue);
  LoadAndExpectError(CreateManifest(R"(["valid_token", ["valid_token"]])"),
                     manifest_errors::kInvalidTrialTokensValue);
}

TEST_F(TrialTokensManifestTest, NoTrialTokens) {
  auto good = LoadAndExpectSuccess(CreateManifestNoTrialTokens());
  EXPECT_FALSE(TrialTokens::HasTrialTokens(*good.get()));
  EXPECT_EQ(TrialTokens::GetTrialTokens(*good.get()), nullptr);
}

TEST_F(TrialTokensManifestTest, VerifyParse) {
  auto good = LoadAndExpectSuccess(CreateManifest(R"(["valid_token"])"));
  EXPECT_TRUE(TrialTokens::HasTrialTokens(*good.get()));

  auto* tokens = TrialTokens::GetTrialTokens(*good.get());
  EXPECT_EQ(1u, tokens->size());
  EXPECT_TRUE(tokens->contains("valid_token"));
}

// TODO(crbug.com/1484767): remove this test before launch to stable
TEST_F(TrialTokensManifestTest, NotAvailableInStable) {
  ScopedCurrentChannel channel{version_info::Channel::STABLE};

  auto good = LoadAndExpectSuccess(CreateManifest(R"(["valid_token"])"));
  EXPECT_FALSE(TrialTokens::HasTrialTokens(*good.get()));
}

}  // namespace extensions

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class MockConsoleLogger final : public GarbageCollected<MockConsoleLogger>,
                                public ConsoleLogger {
 public:
  const String& Message() const { return message_; }

 private:
  void AddConsoleMessageImpl(
      mojom::ConsoleMessageSource,
      mojom::ConsoleMessageLevel,
      const String& message,
      bool discard_duplicates,
      std::optional<mojom::ConsoleMessageCategory>) override {
    message_ = message;
  }
  void AddConsoleMessageImpl(ConsoleMessage*, bool) override {
    NOTREACHED_IN_MIGRATION();
  }
  String message_;
};

}  // namespace

TEST(ScriptWebBundleRuleTest, Empty) {
  auto result =
      ScriptWebBundleRule::ParseJson("", KURL("https://example.com/"), nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kSyntaxError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: invalid JSON.");
}

TEST(ScriptWebBundleRuleTest, Basic) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "scopes": ["js"],
        "resources": ["dir/a.css", "dir/b.css"]
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_THAT(rule.scope_urls(),
              testing::UnorderedElementsAre("https://example.com/js"));
  EXPECT_THAT(rule.resource_urls(),
              testing::UnorderedElementsAre("https://example.com/dir/a.css",
                                            "https://example.com/dir/b.css"));
}

TEST(ScriptWebBundleRuleTest, SourceOnly) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_TRUE(rule.scope_urls().empty());
  EXPECT_TRUE(rule.resource_urls().empty());
}

TEST(ScriptWebBundleRuleTest, ResourcesShouldBeResolvedOnBundleURL) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "hello/foo.wbn",
        "resources": ["dir/a.css"]
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/hello/foo.wbn");
  EXPECT_THAT(rule.resource_urls(), testing::UnorderedElementsAre(
                                        "https://example.com/hello/dir/a.css"));
}

TEST(ScriptWebBundleRuleTest, ScopesShouldBeResolvedOnBundleURL) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "hello/foo.wbn",
        "scopes": ["js"]
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/hello/foo.wbn");
  EXPECT_THAT(rule.scope_urls(),
              testing::UnorderedElementsAre("https://example.com/hello/js"));
}

TEST(ScriptWebBundleRuleTest, CredentialsDefaultIsSameOrigin) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_EQ(rule.credentials_mode(),
            network::mojom::CredentialsMode::kSameOrigin);
}

TEST(ScriptWebBundleRuleTest, CredentialsSameOrigin) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "credentials": "same-origin"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_EQ(rule.credentials_mode(),
            network::mojom::CredentialsMode::kSameOrigin);
}

TEST(ScriptWebBundleRuleTest, CredentialsInclude) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "credentials": "include"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_EQ(rule.credentials_mode(), network::mojom::CredentialsMode::kInclude);
}

TEST(ScriptWebBundleRuleTest, CredentialsOmit) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "credentials": "omit"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_EQ(rule.credentials_mode(), network::mojom::CredentialsMode::kOmit);
}

TEST(ScriptWebBundleRuleTest, CredentialsInvalidValueIsSameOrigin) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "credentials": "invalid-value"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_EQ(rule.credentials_mode(),
            network::mojom::CredentialsMode::kSameOrigin);
}

TEST(ScriptWebBundleRuleTest, CredentialsExtraSpeceIsNotAllowed) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "credentials": " include"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_EQ(rule.credentials_mode(),
            network::mojom::CredentialsMode::kSameOrigin);
}

TEST(ScriptWebBundleRuleTest, CredentialsIsCaseSensitive) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "credentials": "INCLUDE"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_EQ(rule.credentials_mode(),
            network::mojom::CredentialsMode::kSameOrigin);
}

TEST(ScriptWebBundleRuleTest, TopLevelIsNotAnObject) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson("[]", base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kTypeError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: not an object.");
}

TEST(ScriptWebBundleRuleTest, MissingSource) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson("{}", base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kTypeError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: \"source\" "
            "top-level key must be a string.");
}

TEST(ScriptWebBundleRuleTest, WrongSourceType) {
  const KURL base_url("https://example.com/");
  auto result =
      ScriptWebBundleRule::ParseJson(R"({"source": 123})", base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kTypeError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: \"source\" "
            "top-level key must be a string.");
}

TEST(ScriptWebBundleRuleTest, BadSourceURL) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(R"({"source": "http://"})",
                                               base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kTypeError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: \"source\" "
            "is not parsable as a URL.");
}

TEST(ScriptWebBundleRuleTest, NoScopesNorResources) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(R"({"source": "http://"})",
                                               base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kTypeError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: \"source\" "
            "is not parsable as a URL.");
}

TEST(ScriptWebBundleRuleTest, InvalidScopesType) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "scopes": "js"
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kTypeError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: \"scopes\" must be an array.");
}

TEST(ScriptWebBundleRuleTest, InvalidResourcesType) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "resources":  { "a": "hello" }
      })",
      base_url, nullptr);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleError>(result));
  auto& error = absl::get<ScriptWebBundleError>(result);
  EXPECT_EQ(error.GetType(), ScriptWebBundleError::Type::kTypeError);
  EXPECT_EQ(error.GetMessage(),
            "Failed to parse web bundle rule: \"resources\" must be an array.");
}

TEST(ScriptWebBundleRuleTest, UnknownKey) {
  const KURL base_url("https://example.com/");
  MockConsoleLogger* logger = MakeGarbageCollected<MockConsoleLogger>();
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "unknown": []
      })",
      base_url, logger);
  ASSERT_TRUE(absl::holds_alternative<ScriptWebBundleRule>(result));
  auto& rule = absl::get<ScriptWebBundleRule>(result);
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_TRUE(rule.scope_urls().empty());
  EXPECT_TRUE(rule.resource_urls().empty());
  EXPECT_EQ(logger->Message(),
            "Invalid top-level key \"unknown\" in WebBundle rule.");
}

}  // namespace blink

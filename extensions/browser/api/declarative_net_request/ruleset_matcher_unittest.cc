// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <array>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;

using RulesetMatcherTest = ExtensionsTest;

// Tests a simple blocking rule.
TEST_F(RulesetMatcherTest, BlockingRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_block_request = [&matcher](const RequestParams& params) {
    auto action =
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest);
    return action.has_value() && action->IsBlockOrCollapse();
  };

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;
  EXPECT_TRUE(should_block_request(params));

  params.url = &yahoo_url;
  EXPECT_FALSE(should_block_request(params));
}

// Tests a simple redirect rule.
TEST_F(RulesetMatcherTest, RedirectRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("http://yahoo.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  std::optional<RequestAction> redirect_action =
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest);
  ASSERT_TRUE(redirect_action);
  ASSERT_EQ(redirect_action->type, RequestAction::Type::REDIRECT);
  EXPECT_EQ(yahoo_url, redirect_action->redirect_url);

  params.url = &yahoo_url;
  EXPECT_FALSE(
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
}

// Test that a URL cannot redirect to itself, as filed in crbug.com/40623953.
TEST_F(RulesetMatcherTest, PreventSelfRedirect) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("go*");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("http://google.com");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL url("http://google.com");
  RequestParams params;
  params.url = &url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  EXPECT_FALSE(
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
}

// Tests that redirect rules are not applied to WebSocket or WebTransport
// handshakes since the underlying request stack does not support redirects for
// these connection types.
TEST_F(RulesetMatcherTest, RedirectIgnoredForWebSocketAndWebTransport) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.condition->resource_types =
      std::vector<std::string>({"xmlhttprequest", "websocket", "webtransport"});
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("https://other.com/");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL https_url("https://example.com/");
  RequestParams params;
  params.url = &https_url;
  params.is_third_party = true;

  // Check that the rule matches an ordinary request.
  params.element_type = url_pattern_index::flat::ElementType_XMLHTTPREQUEST;
  std::optional<RequestAction> action =
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest);
  ASSERT_TRUE(action);
  EXPECT_EQ(RequestAction::Type::REDIRECT, action->type);

  params.element_type = url_pattern_index::flat::ElementType_WEBTRANSPORT;
  EXPECT_FALSE(
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));

  GURL wss_url("wss://example.com/");
  params.url = &wss_url;
  params.element_type = url_pattern_index::flat::ElementType_WEBSOCKET;
  EXPECT_FALSE(
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
}

// Tests a simple upgrade scheme rule.
TEST_F(RulesetMatcherTest, UpgradeRule) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("upgradeScheme");

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  auto should_upgrade_request = [&matcher](const RequestParams& params) {
    auto action =
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest);
    return action.has_value() && action->type == RequestAction::Type::UPGRADE;
  };

  GURL google_url("http://google.com");
  GURL yahoo_url("http://yahoo.com");
  GURL non_upgradeable_url("https://google.com");

  RequestParams params;
  params.url = &google_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  EXPECT_TRUE(should_upgrade_request(params));

  params.url = &yahoo_url;
  EXPECT_FALSE(should_upgrade_request(params));

  params.url = &non_upgradeable_url;
  EXPECT_FALSE(should_upgrade_request(params));
}

// Tests that a modified ruleset file fails verification.
TEST_F(RulesetMatcherTest, FailedVerification) {
  FileBackedRulesetSource source = CreateTemporarySource();
  std::unique_ptr<RulesetMatcher> matcher;
  int expected_checksum;
  ASSERT_TRUE(CreateVerifiedMatcher({}, source, &matcher, &expected_checksum));

  // Persist invalid data to the ruleset file and ensure that a version mismatch
  // occurs.
  std::string data = "invalid data";
  ASSERT_TRUE(base::WriteFile(source.indexed_path(), data));
  EXPECT_EQ(LoadRulesetResult::kErrorVersionMismatch,
            source.CreateVerifiedMatcher(expected_checksum, &matcher));

  // Now, persist invalid data to the ruleset file, while maintaining the
  // correct version header. Ensure that it fails verification due to checksum
  // mismatch.
  data = GetVersionHeaderForTesting() + "invalid data";
  ASSERT_TRUE(base::WriteFile(source.indexed_path(), data));
  EXPECT_EQ(LoadRulesetResult::kErrorChecksumMismatch,
            source.CreateVerifiedMatcher(expected_checksum, &matcher));
}

TEST_F(RulesetMatcherTest, ModifyHeaders_IsExtraHeaderMatcher) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  EXPECT_FALSE(matcher->IsExtraHeadersMatcher());

  rule.action->type = std::string("modifyHeaders");
  rule.action->response_headers = std::vector<TestHeaderInfo>(
      {TestHeaderInfo("HEADER3", "append", "VALUE3")});
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());
}

TEST_F(RulesetMatcherTest, ModifyHeaders) {
  TestRule rule_1 = CreateGenericRule();
  rule_1.id = kMinValidID;
  rule_1.priority = kMinValidPriority + 1;
  rule_1.condition->url_filter = std::string("example.com");
  rule_1.action->type = std::string("modifyHeaders");
  rule_1.action->request_headers = std::vector<TestHeaderInfo>(
      {TestHeaderInfo("header1", "remove", std::nullopt)});

  TestRule rule_2 = CreateGenericRule();
  rule_2.id = kMinValidID + 1;
  rule_2.priority = kMinValidPriority;
  rule_2.condition->url_filter = std::string("example.com");
  rule_2.action->type = std::string("modifyHeaders");
  rule_2.action->request_headers = std::vector<TestHeaderInfo>(
      {TestHeaderInfo("header1", "set", "value1"),
       TestHeaderInfo("header2", "remove", std::nullopt)});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1, rule_2}, CreateTemporarySource(),
                                    &matcher));
  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  std::vector<RequestAction> modify_header_actions =
      matcher->GetModifyHeadersActions(
          params, RulesetMatchingStage::kOnBeforeRequest, /*min_priority=*/0u);

  RequestAction expected_rule_1_action = CreateRequestActionForTesting(
      RequestAction::Type::MODIFY_HEADERS, *rule_1.id, *rule_1.priority);
  expected_rule_1_action.request_headers_to_modify = {RequestAction::HeaderInfo(
      "header1", dnr_api::HeaderOperation::kRemove, std::nullopt)};

  RequestAction expected_rule_2_action = CreateRequestActionForTesting(
      RequestAction::Type::MODIFY_HEADERS, *rule_2.id, *rule_2.priority);
  expected_rule_2_action.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HeaderOperation::kSet,
                                "value1"),
      RequestAction::HeaderInfo("header2", dnr_api::HeaderOperation::kRemove,
                                std::nullopt)};

  EXPECT_THAT(modify_header_actions,
              testing::UnorderedElementsAre(
                  testing::Eq(testing::ByRef(expected_rule_1_action)),
                  testing::Eq(testing::ByRef(expected_rule_2_action))));
}

// Tests a rule to redirect to an extension path.
TEST_F(RulesetMatcherTest, RedirectToExtensionPath) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  rule.action->redirect.emplace();
  rule.action->redirect->extension_path = "/path/newfile.js?query#fragment";

  std::unique_ptr<RulesetMatcher> matcher;
  const RulesetID kRulesetId(5);
  const size_t kRuleCountLimit = 10;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {rule}, CreateTemporarySource(kRulesetId, kRuleCountLimit), &matcher));

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;

  std::optional<RequestAction> redirect_action =
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest);

  RequestAction expected_action = CreateRequestActionForTesting(
      RequestAction::Type::REDIRECT, *rule.id, *rule.priority, kRulesetId);
  expected_action.redirect_url =
      GURL("chrome-extension://extensionid/path/newfile.js?query#fragment");

  EXPECT_EQ(expected_action, redirect_action);
}

// Tests a rule to redirect to a static url.
TEST_F(RulesetMatcherTest, RedirectToStaticUrl) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  rule.action->redirect.emplace();
  rule.action->redirect->url = "https://google.com";

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));

  GURL example_url("http://example.com");
  RequestParams params;
  params.url = &example_url;

  std::optional<RequestAction> redirect_action =
      matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest);

  ASSERT_TRUE(redirect_action.has_value());
  EXPECT_EQ(redirect_action->type, RequestAction::Type::REDIRECT);
  GURL expected_redirect_url("https://google.com");
  EXPECT_EQ(expected_redirect_url, redirect_action->redirect_url);
}

// Tests url transformation rules.
TEST_F(RulesetMatcherTest, UrlTransform) {
  struct TestCase {
    std::string url;
    // Valid if a redirect is expected.
    std::optional<std::string> expected_redirect_url;
  };

  std::vector<TestCase> cases;
  std::vector<TestRule> rules;

  auto create_transform_rule = [](size_t id, const std::string& filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter = filter;
    rule.priority = kMinValidPriority;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->transform.emplace();
    return rule;
  };

  TestRule rule = create_transform_rule(1, "||1.com");
  rule.action->redirect->transform->scheme = "https";
  rules.push_back(rule);
  cases.push_back({"http://1.com/path?query", "https://1.com/path?query"});

  rule = create_transform_rule(2, "||2.com");
  rule.action->redirect->transform->scheme = "ftp";
  rule.action->redirect->transform->host = "ftp.host.com";
  rule.action->redirect->transform->port = "70";
  rules.push_back(rule);
  cases.push_back(
      {"http://2.com:100/path?query", "ftp://ftp.host.com:70/path?query"});

  rule = create_transform_rule(3, "||3.com");
  rule.action->redirect->transform->port = "";
  rule.action->redirect->transform->path = "";
  rule.action->redirect->transform->query = "?new_query";
  rule.action->redirect->transform->fragment = "#fragment";
  rules.push_back(rule);
  // The path separator '/' is output even when cleared, except when there is no
  // authority, query and fragment.
  cases.push_back(
      {"http://3.com:100/path?query", "http://3.com/?new_query#fragment"});

  rule = create_transform_rule(4, "||4.com");
  rule.action->redirect->transform->scheme = "http";
  rule.action->redirect->transform->path = " ";
  rules.push_back(rule);
  cases.push_back({"http://4.com/xyz", "http://4.com/%20"});

  rule = create_transform_rule(5, "||5.com");
  rule.action->redirect->transform->path = "/";
  rule.action->redirect->transform->query = "";
  rule.action->redirect->transform->fragment = "#";
  rules.push_back(rule);
  cases.push_back(
      {"http://5.com:100/path?query#fragment", "http://5.com:100/#"});

  rule = create_transform_rule(6, "||6.com");
  rule.action->redirect->transform->path = "/path?query";
  rules.push_back(rule);
  // The "?" in path is url encoded since it's not part of the query.
  cases.push_back({"http://6.com/xyz?q1", "http://6.com/path%3Fquery?q1"});

  rule = create_transform_rule(7, "||7.com");
  rule.action->redirect->transform->username = "new_user";
  rule.action->redirect->transform->password = "new_pass";
  rules.push_back(rule);
  cases.push_back(
      {"http://user@7.com/xyz", "http://new_user:new_pass@7.com/xyz"});

  auto make_query = [](const std::string& key, const std::string& value,
                       const std::optional<bool>& replace_only = std::nullopt) {
    TestRuleQueryKeyValue query;
    query.key = key;
    query.value = value;
    query.replace_only = replace_only;

    return query;
  };

  rule = create_transform_rule(8, "||8.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"r1", "r2"});
  rule.action->redirect->transform->query_transform->add_or_replace_params =
      std::vector<TestRuleQueryKeyValue>(
          {make_query("a1", "#"), make_query("a2", ""),
           make_query("a1", "new2"), make_query("a1", "new3")});
  rules.push_back(rule);
  cases.push_back(
      {"http://8.com/"
       "path?r1&r1=val1&a1=val1&r2=val&x3=val&a1=val2&a2=val&r1=val2",
       "http://8.com/path?a1=%23&x3=val&a1=new2&a2=&a1=new3"});
  cases.push_back({"http://8.com/path?query",
                   "http://8.com/path?query=&a1=%23&a2=&a1=new2&a1=new3"});

  rule = create_transform_rule(9, "||9.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"r1", "r2"});
  rules.push_back(rule);
  // No redirect is performed since the url won't change.
  cases.push_back({"http://9.com/path?query#fragment", std::nullopt});

  rule = create_transform_rule(10, "||10.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->remove_params =
      std::vector<std::string>({"q1"});
  rule.action->redirect->transform->query_transform->add_or_replace_params =
      std::vector<TestRuleQueryKeyValue>({make_query("q1", "new")});
  rules.push_back(rule);
  cases.push_back(
      {"https://10.com/path?q1=1&q1=2&q1=3", "https://10.com/path?q1=new"});

  rule = create_transform_rule(11, "||11.com");
  rule.action->redirect->transform->query_transform.emplace();
  rule.action->redirect->transform->query_transform->add_or_replace_params =
      std::vector<TestRuleQueryKeyValue>(
          {make_query("foo", "bar"), make_query("hello", "world", false),
           make_query("abc", "123", true), make_query("abc", "456", true)});
  rules.push_back(rule);
  cases.push_back(
      {"https://11.com/path", "https://11.com/path?foo=bar&hello=world"});
  cases.push_back({"https://11.com/path?abc=def",
                   "https://11.com/path?abc=123&foo=bar&hello=world"});
  cases.push_back({"https://11.com/path?hello=goodbye&abc=def",
                   "https://11.com/path?hello=world&abc=123&foo=bar"});
  cases.push_back({"https://11.com/path?foo=1&foo=2",
                   "https://11.com/path?foo=bar&foo=2&hello=world"});
  cases.push_back(
      {"https://11.com/path?abc=1&abc=2&abc=3",
       "https://11.com/path?abc=123&abc=456&abc=3&foo=bar&hello=world"});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing url %s", test_case.url.c_str()));

    GURL url(test_case.url);
    ASSERT_TRUE(url.is_valid()) << test_case.url;
    RequestParams params;
    params.url = &url;

    std::optional<RequestAction> redirect_action =
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest);

    if (!test_case.expected_redirect_url) {
      EXPECT_FALSE(redirect_action) << redirect_action->redirect_url->spec();
      continue;
    }

    ASSERT_TRUE(redirect_action.has_value());
    EXPECT_EQ(redirect_action->type, RequestAction::Type::REDIRECT);

    ASSERT_TRUE(GURL(*test_case.expected_redirect_url).is_valid())
        << *test_case.expected_redirect_url;

    ASSERT_TRUE(redirect_action.has_value());
    EXPECT_EQ(GURL(*test_case.expected_redirect_url),
              redirect_action->redirect_url);
  }
}

// Tests regex rules are evaluated correctly for different action types.
TEST_F(RulesetMatcherTest, RegexRules) {
  auto create_regex_rule = [](size_t id, const std::string& regex_filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = regex_filter;
    return rule;
  };

  std::vector<TestRule> rules;

  // Add a blocking rule.
  TestRule block_rule = create_regex_rule(1, R"((?:block|collapse)\.com/path)");
  rules.push_back(block_rule);

  // Add an allowlist rule.
  TestRule allow_rule = create_regex_rule(2, R"(http://(\w+\.)+allow\.com)");
  allow_rule.action->type = "allow";
  rules.push_back(allow_rule);

  // Add a redirect rule.
  TestRule redirect_rule = create_regex_rule(3, R"(redirect\.com)");
  redirect_rule.action->type = "redirect";
  redirect_rule.action->redirect.emplace();
  redirect_rule.priority = kMinValidPriority;
  redirect_rule.action->redirect->url = "https://google.com";
  rules.push_back(redirect_rule);

  // Add a upgrade rule.
  TestRule upgrade_rule = create_regex_rule(4, "upgrade");
  upgrade_rule.action->type = "upgradeScheme";
  upgrade_rule.priority = kMinValidPriority;
  rules.push_back(upgrade_rule);

  // Add a modify headers rule.
  TestRule modify_headers_rule =
      create_regex_rule(6, R"(^(?:http|https)://[a-z\.]+\.org)");
  modify_headers_rule.action->type = "modifyHeaders";
  modify_headers_rule.action->request_headers =
      std::vector<TestHeaderInfo>({TestHeaderInfo("header1", "set", "value1")});
  rules.push_back(modify_headers_rule);

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  struct TestCase {
    const char* url = nullptr;
    std::optional<RequestAction> expected_action;
    std::optional<RequestAction> expected_modify_header_action;
  };

  std::vector<TestCase> test_cases;

  {
    TestCase test_case = {"http://www.block.com/path"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *block_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://www.collapse.com/PATH"};
    // Filters are case insensitive by default, hence the request will match.
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *block_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.xyz.allow.com/path"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::ALLOW, *allow_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://allow.com/path"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://redirect.com?path=abc"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, *redirect_rule.id);
    test_case.expected_action->redirect_url =
        GURL(*redirect_rule.action->redirect->url);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://redirect.eu#query"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/upgrade"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::UPGRADE, *upgrade_rule.id);
    test_case.expected_action->redirect_url.emplace(
        "https://example.com/upgrade");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.org/path"};
    test_case.expected_modify_header_action = CreateRequestActionForTesting(
        RequestAction::Type::MODIFY_HEADERS, *modify_headers_rule.id);
    test_case.expected_modify_header_action->request_headers_to_modify = {
        RequestAction::HeaderInfo("header1", dnr_api::HeaderOperation::kSet,
                                  "value1")};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com"};
    test_cases.push_back(std::move(test_case));
  }

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);

    GURL url(test_case.url);
    RequestParams params;
    params.url = &url;

    EXPECT_EQ(
        test_case.expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));

    std::vector<RequestAction> modify_header_actions =
        matcher->GetModifyHeadersActions(params,
                                         RulesetMatchingStage::kOnBeforeRequest,
                                         /*min_priority=*/0u);

    if (test_case.expected_modify_header_action) {
      EXPECT_THAT(modify_header_actions,
                  testing::ElementsAre(testing::Eq(testing::ByRef(
                      test_case.expected_modify_header_action))));
    } else {
      EXPECT_TRUE(modify_header_actions.empty());
    }
  }

  EXPECT_TRUE(matcher->IsExtraHeadersMatcher());
}

// Ensure that the rule metadata is checked correctly for regex rules.
TEST_F(RulesetMatcherTest, RegexRules_Metadata) {
  auto create_regex_rule = [](size_t id, const std::string& regex_filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = regex_filter;
    return rule;
  };

  std::vector<TestRule> rules;

  // Add a case sensitive rule.
  TestRule path_rule = create_regex_rule(1, "/PATH");
  path_rule.condition->is_url_filter_case_sensitive = true;
  rules.push_back(path_rule);

  // Add a case insensitive rule.
  TestRule xyz_rule = create_regex_rule(2, "/XYZ");
  rules.push_back(xyz_rule);

  // Test `domains`, `excludedDomains`.
  TestRule domains_rule = create_regex_rule(3, "deprecated_domains");
  domains_rule.condition->domains = std::vector<std::string>({"example.com"});
  domains_rule.condition->excluded_domains =
      std::vector<std::string>({"b.example.com"});
  rules.push_back(domains_rule);

  // Test `initiatorDomains`, `excludedInitiatorDomains`.
  TestRule initiator_domains_rule = create_regex_rule(4, "initiator_domains");
  initiator_domains_rule.condition->initiator_domains =
      std::vector<std::string>({"example.com"});
  initiator_domains_rule.condition->excluded_initiator_domains =
      std::vector<std::string>({"b.example.com"});
  rules.push_back(initiator_domains_rule);

  // Test `requestDomains`, `excludedRequestDomains`.
  TestRule request_domains_rule = create_regex_rule(5, "request_domains");
  request_domains_rule.condition->request_domains =
      std::vector<std::string>({"example.com"});
  request_domains_rule.condition->excluded_request_domains =
      std::vector<std::string>({"b.example.com"});
  rules.push_back(request_domains_rule);

  // Test `resourceTypes`.
  TestRule sub_frame_rule = create_regex_rule(6, R"((abc|def)\.com)");
  sub_frame_rule.condition->resource_types =
      std::vector<std::string>({"sub_frame"});
  rules.push_back(sub_frame_rule);

  // Test `domainType`.
  TestRule third_party_rule = create_regex_rule(7, R"(http://(\d+)\.com)");
  third_party_rule.condition->domain_type = "thirdParty";
  rules.push_back(third_party_rule);

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  struct TestCase {
    const char* url = nullptr;
    url::Origin first_party_origin;
    url_pattern_index::flat::ElementType element_type =
        url_pattern_index::flat::ElementType_OTHER;
    bool is_third_party = false;
    std::optional<RequestAction> expected_action;
  };
  std::vector<TestCase> test_cases;

  {
    TestCase test_case = {"http://example.com/path/abc"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/PATH/abc"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *path_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/xyz/abc"};
    test_case.expected_action =
        CreateRequestActionForTesting(RequestAction::Type::BLOCK, *xyz_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/XYZ/abc"};
    test_case.expected_action =
        CreateRequestActionForTesting(RequestAction::Type::BLOCK, *xyz_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/deprecated_domains"};
    test_case.first_party_origin =
        url::Origin::Create(GURL("http://a.example.com"));
    test_case.is_third_party = true;
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *domains_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/deprecated_domains"};
    test_case.first_party_origin =
        url::Origin::Create(GURL("http://b.example.com"));
    test_case.is_third_party = true;
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/initiator_domains"};
    test_case.first_party_origin =
        url::Origin::Create(GURL("http://a.example.com"));
    test_case.is_third_party = true;
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *initiator_domains_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/initiator_domains"};
    test_case.first_party_origin =
        url::Origin::Create(GURL("http://b.example.com"));
    test_case.is_third_party = true;
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/request_domains"};
    test_case.first_party_origin =
        url::Origin::Create(GURL("http://foobar.com"));
    test_case.is_third_party = true;
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *request_domains_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com/request_domains"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *request_domains_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://b.example.com/request_domains"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://foobar.com/request_domains"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.com"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.com"};
    test_case.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::COLLAPSE, *sub_frame_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://243.com"};
    test_case.is_third_party = true;
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::BLOCK, *third_party_rule.id);
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://243.com"};
    test_case.first_party_origin = url::Origin::Create(GURL(test_case.url));
    test_case.is_third_party = false;
    test_cases.push_back(std::move(test_case));
  }

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Case number-%" PRIuS " url-%s", i,
                                    test_cases[i].url));

    GURL url(test_cases[i].url);
    RequestParams params;
    params.url = &url;
    params.first_party_origin = test_cases[i].first_party_origin;
    params.element_type = test_cases[i].element_type;
    params.is_third_party = test_cases[i].is_third_party;

    EXPECT_EQ(
        test_cases[i].expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
  }
}

// Ensures that RulesetMatcher combines the results of regex and filter-list
// style redirect rules correctly.
TEST_F(RulesetMatcherTest, RegexAndFilterListRules_RedirectPriority) {
  struct RuleInfo {
    size_t id;
    size_t priority;
    const char* action_type;
    const char* filter;
    bool is_regex_rule;
    std::optional<std::string> redirect_url;
  };
  auto rule_info = std::to_array<RuleInfo>({
      {1, 1, "redirect", "filter.com", false, "http://redirect_filter.com"},
      {2, 1, "upgradeScheme", "regex\\.com", true, std::nullopt},
      {3, 9, "redirect", "common1.com", false, "http://common1_filter.com"},
      {4, 10, "redirect", "common1\\.com", true, "http://common1_regex.com"},
      {5, 10, "upgradeScheme", "common2.com", false, std::nullopt},
      {6, 9, "upgradeScheme", "common2\\.com", true, std::nullopt},
      {7, 10, "redirect", "abc\\.com", true, "http://example1.com"},
      {8, 9, "redirect", "abc", true, "http://example2.com"},
  });

  std::vector<TestRule> rules;
  for (const auto& info : rule_info) {
    TestRule rule = CreateGenericRule();
    rule.id = info.id;
    rule.priority = info.priority;
    rule.action->type = info.action_type;

    rule.condition->url_filter.reset();
    if (info.is_regex_rule) {
      rule.condition->regex_filter = info.filter;
    } else {
      rule.condition->url_filter = info.filter;
    }

    if (info.redirect_url) {
      rule.action->redirect.emplace();
      rule.action->redirect->url = info.redirect_url;
    }

    rules.push_back(rule);
  }

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  struct TestCase {
    const char* url = nullptr;
    std::optional<RequestAction> expected_action;
  };
  std::vector<TestCase> test_cases;

  {
    TestCase test_case = {"http://filter.com"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[0].id, rule_info[0].priority);
    test_case.expected_action->redirect_url.emplace(
        "http://redirect_filter.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://regex.com"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::UPGRADE, rule_info[1].id, rule_info[1].priority);
    test_case.expected_action->redirect_url.emplace("https://regex.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://common1.com"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[3].id, rule_info[3].priority);
    test_case.expected_action->redirect_url.emplace("http://common1_regex.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://common2.com"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::UPGRADE, rule_info[4].id, rule_info[4].priority);
    test_case.expected_action->redirect_url.emplace("https://common2.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"https://common2.com"};
    // No action since request is not upgradeable.
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://example.com"};
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://abc.com"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[6].id, rule_info[6].priority);
    test_case.expected_action->redirect_url.emplace("http://example1.com");
    test_cases.push_back(std::move(test_case));
  }

  {
    TestCase test_case = {"http://xyz.com/abc"};
    test_case.expected_action = CreateRequestActionForTesting(
        RequestAction::Type::REDIRECT, rule_info[7].id, rule_info[7].priority);
    test_case.expected_action->redirect_url.emplace("http://example2.com");
    test_cases.push_back(std::move(test_case));
  }

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);

    GURL url(test_case.url);
    RequestParams params;
    params.url = &url;

    EXPECT_EQ(
        test_case.expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
  }
}

TEST_F(RulesetMatcherTest, RegexAndFilterListRules_ModifyHeaders) {
  std::vector<TestRule> rules;

  TestRule rule = CreateGenericRule();
  rule.id = 1;
  rule.priority = kMinValidPriority + 1;
  rule.action->type = "modifyHeaders";
  rule.condition->url_filter = "abc";
  rule.action->request_headers = std::vector<TestHeaderInfo>(
      {TestHeaderInfo("header1", "remove", std::nullopt),
       TestHeaderInfo("header2", "remove", std::nullopt)});
  rules.push_back(rule);

  RequestAction action_1 = CreateRequestActionForTesting(
      RequestAction::Type::MODIFY_HEADERS, 1, *rule.priority);
  action_1.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HeaderOperation::kRemove,
                                std::nullopt),
      RequestAction::HeaderInfo("header2", dnr_api::HeaderOperation::kRemove,
                                std::nullopt)};

  rule = CreateGenericRule();
  rule.id = 2;
  rule.priority = kMinValidPriority;
  rule.condition->url_filter.reset();
  rule.condition->regex_filter = "example";
  rule.action->type = "modifyHeaders";
  rule.action->request_headers = std::vector<TestHeaderInfo>(
      {TestHeaderInfo("header1", "remove", std::nullopt),
       TestHeaderInfo("header3", "remove", std::nullopt)});
  rules.push_back(rule);

  RequestAction action_2 = CreateRequestActionForTesting(
      RequestAction::Type::MODIFY_HEADERS, 2, *rule.priority);
  action_2.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HeaderOperation::kRemove,
                                std::nullopt),
      RequestAction::HeaderInfo("header3", dnr_api::HeaderOperation::kRemove,
                                std::nullopt)};

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  {
    GURL url("http://nomatch.com");
    SCOPED_TRACE(url.spec());
    RequestParams params;
    params.url = &url;

    EXPECT_TRUE(matcher
                    ->GetModifyHeadersActions(
                        params, RulesetMatchingStage::kOnBeforeRequest,
                        /*min_priority=*/0u)
                    .empty());
  }

  {
    GURL url("http://abc.com");
    SCOPED_TRACE(url.spec());
    RequestParams params;
    params.url = &url;

    std::vector<RequestAction> actions = matcher->GetModifyHeadersActions(
        params, RulesetMatchingStage::kOnBeforeRequest, /*min_priority=*/0u);
    EXPECT_THAT(actions, testing::UnorderedElementsAre(
                             testing::Eq(testing::ByRef(action_1))));
  }

  {
    GURL url("http://example.com");
    SCOPED_TRACE(url.spec());
    RequestParams params;
    params.url = &url;

    std::vector<RequestAction> actions = matcher->GetModifyHeadersActions(
        params, RulesetMatchingStage::kOnBeforeRequest, /*min_priority=*/0u);
    EXPECT_THAT(actions, testing::UnorderedElementsAre(
                             testing::Eq(testing::ByRef(action_2))));
  }

  {
    // Send a request that matches both the filter list and regex rules.
    GURL url("http://abc.com/example");
    SCOPED_TRACE(url.spec());
    RequestParams params;
    params.url = &url;

    std::vector<RequestAction> actions = matcher->GetModifyHeadersActions(
        params, RulesetMatchingStage::kOnBeforeRequest, /*min_priority=*/0u);
    EXPECT_THAT(actions, testing::UnorderedElementsAre(
                             testing::Eq(testing::ByRef(action_1)),
                             testing::Eq(testing::ByRef(action_2))));

    // GetModifyHeadersActions specifies a minimum priority greater than the
    // rules' priority, so no actions should be returned.
    EXPECT_TRUE(matcher
                    ->GetModifyHeadersActions(
                        params, RulesetMatchingStage::kOnBeforeRequest,
                        /*min_priority=*/std::numeric_limits<uint64_t>::max())
                    .empty());
  }
}

// Tests that regex substitution works correctly.
TEST_F(RulesetMatcherTest, RegexSubstitution) {
  struct {
    int id;
    std::string regex_filter;
    std::string regex_substitution;
  } rule_info[] = {
      // "\0" captures the complete matched string.
      {1, R"(^.*google\.com.*$)", R"(https://redirect.com?original=\0)"},
      {2, R"(http://((?:abc|def)\.xyz.com.*)$)", R"(https://www.\1)"},
      {3, R"(^(http|https)://example\.com.*(\?|&)redirect=(.*?)(?:&|$).*$)",
       R"(\1://\3)"},
      {4, R"(reddit\.com)", "abc.com"},
      {5, R"(^http://www\.(pqr|rst)\.xyz\.com)", R"(https://\1.xyz.com)"},
      {6, R"(\.in)", ".co.in"},
  };

  std::vector<TestRule> rules;
  for (const auto& info : rule_info) {
    TestRule rule = CreateGenericRule();
    rule.id = info.id;
    rule.priority = kMinValidPriority;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = info.regex_filter;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->regex_substitution = info.regex_substitution;
    rules.push_back(rule);
  }

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

  auto create_redirect_action = [](int rule_id, std::string redirect_url) {
    RequestAction action =
        CreateRequestActionForTesting(RequestAction::Type::REDIRECT, rule_id);
    action.redirect_url.emplace(redirect_url);
    return action;
  };

  struct {
    std::string url;
    std::optional<RequestAction> expected_action;
  } test_cases[] = {
      {"http://google.com/path?query",
       create_redirect_action(
           1, "https://redirect.com?original=http://google.com/path?query")},
      {"http://def.xyz.com/path?query",
       create_redirect_action(2, "https://www.def.xyz.com/path?query")},
      {"http://example.com/path?q1=1&redirect=facebook.com&q2=2",
       create_redirect_action(3, "http://facebook.com")},
      // The redirect url here would have been "http://" which is invalid.
      {"http://example.com/path?q1=1&redirect=&q2=2", std::nullopt},
      {"https://reddit.com", create_redirect_action(4, "https://abc.com")},
      {"http://www.rst.xyz.com/path",
       create_redirect_action(5, "https://rst.xyz.com/path")},
      {"http://yahoo.in/path",
       create_redirect_action(6, "http://yahoo.co.in/path")},
      // No match.
      {"http://example.com", std::nullopt}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);

    GURL url(test_case.url);
    CHECK(url.is_valid());
    RequestParams params;
    params.url = &url;

    ASSERT_EQ(
        test_case.expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
  }
}

TEST_F(RulesetMatcherTest, RulesCount) {
  size_t kNumNonRegexRules = 20;
  size_t kNumRegexRules = 10;

  // Rules that are not block or allow rules are considered unsafe for dynamic
  // rulesets. Make some of the rules these types as well.
  size_t kNumUnsafeNonRegexRules = 5;
  size_t kNumUnsafeRegexRules = 3;

  std::vector<TestRule> rules;
  int id = kMinValidID;
  for (size_t i = 0; i < kNumNonRegexRules; ++i, ++id) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter = base::NumberToString(id);
    if (i < kNumUnsafeNonRegexRules) {
      rule.action->type = "redirect";
      rule.action->redirect.emplace();
      rule.action->redirect->url = "http://google.com";
    }
    rules.push_back(rule);
  }

  for (size_t i = 0; i < kNumRegexRules; ++i, ++id) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = base::NumberToString(id);
    if (i < kNumUnsafeRegexRules) {
      rule.action->type = std::string("modifyHeaders");
      rule.action->response_headers = std::vector<TestHeaderInfo>(
          {TestHeaderInfo("HEADER3", "append", "VALUE3")});
    }
    rules.push_back(rule);
  }

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);
  EXPECT_EQ(kNumRegexRules + kNumNonRegexRules, matcher->GetRulesCount());
  // For static rulesets, no rules are considered unsafe.
  EXPECT_EQ(std::nullopt, matcher->GetUnsafeRulesCount());
  EXPECT_EQ(kNumRegexRules, matcher->GetRegexRulesCount());

  // Recreate `matcher` for a dynamic ruleset to test that unsafe rule count is
  // calculated correctly.
  ASSERT_TRUE(CreateVerifiedMatcher(
      rules, CreateTemporarySource(kDynamicRulesetID), &matcher));
  ASSERT_TRUE(matcher);
  EXPECT_EQ(kNumRegexRules + kNumNonRegexRules, matcher->GetRulesCount());
  EXPECT_EQ(kNumUnsafeNonRegexRules + kNumUnsafeRegexRules,
            matcher->GetUnsafeRulesCount());
  EXPECT_EQ(kNumRegexRules, matcher->GetRegexRulesCount());

  // Also verify the rules count for an empty matcher.
  ASSERT_TRUE(
      CreateVerifiedMatcher({} /* rules */, CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);
  EXPECT_EQ(0u, matcher->GetRulesCount());
  EXPECT_EQ(std::nullopt, matcher->GetUnsafeRulesCount());
  EXPECT_EQ(0u, matcher->GetRegexRulesCount());
}

// Test that rules with the same priority will override each other correctly
// based on action.
TEST_F(RulesetMatcherTest, BreakTiesByActionPriority) {
  struct {
    int rule_id;
    std::string rule_action;

    // The expected action, assuming this rule and all previous rules match and
    // have the same priority.
    RequestAction::Type expected_action;
    // The ID of the rule expected to match.
    int expected_rule_id = 0;
  } test_cases[] = {
      {1, "redirect", RequestAction::Type::REDIRECT, 1},
      {2, "upgradeScheme", RequestAction::Type::UPGRADE, 2},
      {3, "block", RequestAction::Type::BLOCK, 3},
      {4, "allowAllRequests", RequestAction::Type::ALLOW_ALL_REQUESTS, 4},
      {5, "allow", RequestAction::Type::ALLOW, 5},
      {6, "block", RequestAction::Type::ALLOW, 5},
  };

  std::vector<TestRule> rules;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.rule_action);

    TestRule rule = CreateGenericRule();
    rule.id = test_case.rule_id;
    rule.priority = kMinValidPriority;
    rule.condition->url_filter = "http://example.com";
    rule.action->type = test_case.rule_action;
    rule.condition->resource_types = std::vector<std::string>{"main_frame"};
    if (test_case.rule_action == "redirect") {
      rule.action->redirect.emplace();
      rule.action->redirect->url = "http://google.com";
    }
    rules.push_back(rule);

    std::unique_ptr<RulesetMatcher> matcher;
    ASSERT_TRUE(
        CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));

    GURL url("http://example.com");
    RequestParams params;
    params.url = &url;
    params.element_type = url_pattern_index::flat::ElementType_MAIN_FRAME;

    int expected_rule_id = test_case.expected_rule_id;
    if (expected_rule_id == 0) {
      expected_rule_id = *rule.id;
    }
    RequestAction expected_action = CreateRequestActionForTesting(
        test_case.expected_action, expected_rule_id);
    if (test_case.expected_action == RequestAction::Type::REDIRECT) {
      expected_action.redirect_url = GURL("http://google.com");
    } else if (test_case.expected_action == RequestAction::Type::UPGRADE) {
      expected_action.redirect_url = GURL("https://example.com");
    }

    EXPECT_EQ(
        expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
  }
}

// Test fixture to test allowAllRequests rules. We inherit from ExtensionsTest
// to ensure we can work with WebContentsTester and associated classes.
using AllowAllRequestsTest = ExtensionsTest;

// Tests that we track allowlisted frames (frames matching allowAllRequests
// rules) correctly.
TEST_F(AllowAllRequestsTest, AllowlistedFrameTracking) {
  TestRule google_rule_1 = CreateGenericRule();
  google_rule_1.id = kMinValidID;
  google_rule_1.condition->url_filter = "google.com/xyz";
  google_rule_1.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  google_rule_1.action->type = std::string("allowAllRequests");
  google_rule_1.priority = 2;

  TestRule google_rule_2 = CreateGenericRule();
  google_rule_2.id = kMinValidID + 1;
  google_rule_2.condition->url_filter.reset();
  google_rule_2.condition->regex_filter = "xyz";
  google_rule_2.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  google_rule_2.action->type = std::string("allowAllRequests");
  google_rule_2.priority = 3;

  TestRule example_rule = CreateGenericRule();
  example_rule.id = kMinValidID + 2;
  example_rule.condition->url_filter.reset();
  example_rule.condition->regex_filter = std::string("example");
  example_rule.condition->resource_types =
      std::vector<std::string>({"sub_frame"});
  example_rule.action->type = std::string("allowAllRequests");
  example_rule.priority = 4;

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(
      CreateVerifiedMatcher({google_rule_1, google_rule_2, example_rule},
                            CreateTemporarySource(), &matcher));

  auto simulate_navigation = [&matcher](content::RenderFrameHost* host,
                                        const GURL& url) {
    content::RenderFrameHost* new_host =
        content::NavigationSimulator::NavigateAndCommitFromDocument(url, host);
    EXPECT_TRUE(new_host);

    // Note |host| might have been freed by now.
    testing::NiceMock<content::MockNavigationHandle> navigation_handle(
        url, new_host);
    navigation_handle.set_has_committed(true);
    matcher->OnDidFinishNavigation(&navigation_handle,
                                   /*has_host_access_for_navigation=*/true);

    return new_host;
  };
  auto simulate_frame_destroyed = [&matcher](content::RenderFrameHost* host) {
    matcher->OnRenderFrameDeleted(host);
  };

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);

  GURL example_url("http://example.com");
  simulate_navigation(web_contents->GetPrimaryMainFrame(), example_url);
  std::optional<RequestAction> action =
      matcher->GetAllowlistedFrameActionForTesting(
          web_contents->GetPrimaryMainFrame());
  EXPECT_FALSE(action);

  GURL google_url_1("http://google.com/xyz");
  simulate_navigation(web_contents->GetPrimaryMainFrame(), google_url_1);
  action = matcher->GetAllowlistedFrameActionForTesting(
      web_contents->GetPrimaryMainFrame());
  RequestAction google_rule_2_action =
      CreateRequestActionForTesting(RequestAction::Type::ALLOW_ALL_REQUESTS,
                                    *google_rule_2.id, *google_rule_2.priority);
  EXPECT_EQ(google_rule_2_action, action);

  auto* render_frame_host_tester =
      content::RenderFrameHostTester::For(web_contents->GetPrimaryMainFrame());
  content::RenderFrameHost* child =
      render_frame_host_tester->AppendChild("sub_frame");
  ASSERT_TRUE(child);

  child = simulate_navigation(child, example_url);
  action = matcher->GetAllowlistedFrameActionForTesting(child);
  RequestAction example_rule_action =
      CreateRequestActionForTesting(RequestAction::Type::ALLOW_ALL_REQUESTS,
                                    *example_rule.id, *example_rule.priority);
  EXPECT_EQ(example_rule_action, action);

  GURL yahoo_url("http://yahoo.com");
  child = simulate_navigation(child, yahoo_url);
  action = matcher->GetAllowlistedFrameActionForTesting(child);
  EXPECT_EQ(google_rule_2_action, action);

  simulate_frame_destroyed(child);
  action = matcher->GetAllowlistedFrameActionForTesting(child);
  EXPECT_FALSE(action);

  simulate_frame_destroyed(web_contents->GetPrimaryMainFrame());
  action = matcher->GetAllowlistedFrameActionForTesting(
      web_contents->GetPrimaryMainFrame());
  EXPECT_FALSE(action);
}

// Regression test for crbug.com/519999855: verifies that error documents do not
// gain allowAllRequests privileges from their failed destination URL (neither
// on initial error commit nor on subsequent same-document navigations), evict
// stale self-allowlist entries on same-RFH error commits, and still preserve
// any allowlist action inherited from an enclosing allowlisted parent frame.
TEST_F(AllowAllRequestsTest, ErrorDocumentTracking) {
  TestRule parent_allow_rule = CreateGenericRule();
  parent_allow_rule.id = kMinValidID;
  parent_allow_rule.condition->url_filter = "||parent-allowed.com";
  parent_allow_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  parent_allow_rule.action->type = "allowAllRequests";
  parent_allow_rule.priority = 2;

  TestRule subframe_allow_rule = CreateGenericRule();
  subframe_allow_rule.id = kMinValidID + 1;
  subframe_allow_rule.condition->url_filter = "||corp-intranet.com";
  subframe_allow_rule.condition->resource_types =
      std::vector<std::string>({"sub_frame"});
  subframe_allow_rule.action->type = "allowAllRequests";
  subframe_allow_rule.priority = 10;

  TestRule block_rule = CreateGenericRule();
  block_rule.id = kMinValidID + 2;
  block_rule.condition->url_filter = "||tracker.com";
  block_rule.action->type = "block";
  block_rule.priority = 5;

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {parent_allow_rule, subframe_allow_rule, block_rule},
      CreateTemporarySource(), &matcher));

  auto notify_committed = [&matcher](content::RenderFrameHost* host,
                                     const GURL& url, bool is_error_page) {
    testing::NiceMock<content::MockNavigationHandle> navigation_handle(url,
                                                                       host);
    navigation_handle.set_has_committed(true);
    navigation_handle.set_is_error_page(is_error_page);
    matcher->OnDidFinishNavigation(&navigation_handle,
                                   /*has_host_access_for_navigation=*/true);
  };

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);

  // 1. Navigate main frame to a non-allowlisted page and append a child frame.
  const GURL untrusted_main_url("https://attacker.com/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), untrusted_main_url);
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  notify_committed(main_rfh, untrusted_main_url, /*is_error_page=*/false);
  EXPECT_FALSE(matcher->GetAllowlistedFrameActionForTesting(main_rfh));

  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("sub_frame");
  ASSERT_TRUE(child);

  // 2. Navigate child frame to https://corp-intranet.com/ and fail (e.g.,
  // ERR_BLOCKED_BY_CSP). Even though GetLastCommittedURL() holds
  // https://corp-intranet.com/, the error document must not be allowlisted and
  // block rules must still apply to its subresource loads.
  const GURL blocked_allowlisted_url("https://corp-intranet.com/secret");
  content::RenderFrameHost* error_child =
      content::NavigationSimulator::NavigateAndFailFromDocument(
          blocked_allowlisted_url, net::ERR_BLOCKED_BY_CSP, child);
  ASSERT_TRUE(error_child);
  ASSERT_TRUE(error_child->IsErrorDocument());
  ASSERT_EQ(blocked_allowlisted_url, error_child->GetLastCommittedURL());

  notify_committed(error_child, blocked_allowlisted_url,
                   /*is_error_page=*/true);
  EXPECT_FALSE(matcher->GetAllowlistedFrameActionForTesting(error_child));

  // Verify subresource requests from `error_child` are blocked by `block_rule`.
  GURL tracker_url("https://tracker.com/probe.gif");
  RequestParams subresource_params;
  subresource_params.url = &tracker_url;
  subresource_params.element_type = url_pattern_index::flat::ElementType_IMAGE;
  subresource_params.parent_routing_id = error_child->GetGlobalId();
  EXPECT_EQ(CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                          *block_rule.id, *block_rule.priority),
            matcher->GetAction(subresource_params,
                               RulesetMatchingStage::kOnBeforeRequest));

  // 3. Simulate a same-document navigation inside `error_child` (where
  // NavigationHandle::IsErrorPage() is false while host->IsErrorDocument()
  // remains true). The error document must still not be allowlisted.
  const GURL fragment_url("https://corp-intranet.com/secret#fragment");
  notify_committed(error_child, fragment_url, /*is_error_page=*/false);
  EXPECT_FALSE(matcher->GetAllowlistedFrameActionForTesting(error_child));

  // 4. Commit a valid navigation to https://corp-intranet.com/ in the subframe,
  // then simulate an error page committing in the same RenderFrameHost to
  // verify the stale self-allowlist entry is evicted.
  content::RenderFrameHost* allowed_child =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          blocked_allowlisted_url, error_child);
  ASSERT_TRUE(allowed_child);
  ASSERT_FALSE(allowed_child->IsErrorDocument());
  notify_committed(allowed_child, blocked_allowlisted_url,
                   /*is_error_page=*/false);
  EXPECT_EQ(CreateRequestActionForTesting(
                RequestAction::Type::ALLOW_ALL_REQUESTS,
                *subframe_allow_rule.id, *subframe_allow_rule.priority),
            matcher->GetAllowlistedFrameActionForTesting(allowed_child));

  // Simulate an error commit in the same RenderFrameHost. This intentionally
  // exercises the NavigationHandle::IsErrorPage() branch while the host's last
  // committed URL still matches `subframe_allow_rule`, verifying that the stale
  // self-allowlist entry is evicted rather than re-added.
  notify_committed(allowed_child, blocked_allowlisted_url,
                   /*is_error_page=*/true);
  EXPECT_FALSE(matcher->GetAllowlistedFrameActionForTesting(allowed_child));

  // 5. Navigate main frame to https://parent-allowed.com/ (priority 2) and fail
  // a child subframe navigation to https://corp-intranet.com/ (priority 10):
  // the error child must inherit the parent's priority-2 action rather than
  // gaining the failed URL's priority-10 action.
  const GURL allowed_main_url("https://parent-allowed.com/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents.get(),
                                                             allowed_main_url);
  main_rfh = web_contents->GetPrimaryMainFrame();
  notify_committed(main_rfh, allowed_main_url, /*is_error_page=*/false);
  RequestAction parent_action = CreateRequestActionForTesting(
      RequestAction::Type::ALLOW_ALL_REQUESTS, *parent_allow_rule.id,
      *parent_allow_rule.priority);
  EXPECT_EQ(parent_action,
            matcher->GetAllowlistedFrameActionForTesting(main_rfh));

  child = content::RenderFrameHostTester::For(main_rfh)->AppendChild("sub2");
  ASSERT_TRUE(child);
  error_child = content::NavigationSimulator::NavigateAndFailFromDocument(
      blocked_allowlisted_url, net::ERR_BLOCKED_BY_CSP, child);
  ASSERT_TRUE(error_child);
  notify_committed(error_child, blocked_allowlisted_url,
                   /*is_error_page=*/true);
  EXPECT_EQ(parent_action,
            matcher->GetAllowlistedFrameActionForTesting(error_child));
}

// Ensures that GetBeforeRequestAction correctly incorporates allowAllRequests
// rules.
TEST_F(AllowAllRequestsTest, GetBeforeRequestAction) {
  struct RuleData {
    int id;
    int priority;
    std::string action_type;
    std::string url_filter;
    bool is_regex_rule;
  };
  auto rule_data = std::to_array<RuleData>({
      {1, 1, "allowAllRequests", "google", true},
      {2, 3, "block", "||match", false},
      {3, 2, "allowAllRequests", "match1", true},
      {4, 4, "allowAllRequests", "match2", false},
  });

  std::vector<TestRule> test_rules;
  for (const auto& rule : rule_data) {
    TestRule test_rule = CreateGenericRule();
    test_rule.id = rule.id;
    test_rule.priority = rule.priority;
    test_rule.action->type = rule.action_type;
    test_rule.condition->url_filter.reset();
    if (rule.is_regex_rule) {
      test_rule.condition->regex_filter = rule.url_filter;
    } else {
      test_rule.condition->url_filter = rule.url_filter;
    }
    if (rule.action_type == "allowAllRequests") {
      test_rule.condition->resource_types =
          std::vector<std::string>({"main_frame", "sub_frame"});
    }

    test_rules.push_back(test_rule);
  }

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(
      CreateVerifiedMatcher(test_rules, CreateTemporarySource(), &matcher));

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);

  GURL google_url("http://google.com");
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(google_url);

  testing::NiceMock<content::MockNavigationHandle> navigation_handle(
      google_url, web_contents->GetPrimaryMainFrame());
  navigation_handle.set_has_committed(true);
  matcher->OnDidFinishNavigation(&navigation_handle,
                                 /*has_host_access_for_navigation=*/true);

  struct {
    std::string url;
    RequestAction expected_action;
  } cases[] = {
      {"http://nomatch.com",
       CreateRequestActionForTesting(RequestAction::Type::ALLOW_ALL_REQUESTS,
                                     rule_data[0].id, rule_data[0].priority)},
      {"http://match1.com",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     rule_data[1].id, rule_data[1].priority)},
      {"http://match2.com",
       CreateRequestActionForTesting(RequestAction::Type::ALLOW_ALL_REQUESTS,
                                     rule_data[3].id, rule_data[3].priority)},
  };

  // Simulate sub-frame requests.
  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.url);
    RequestParams params;

    GURL url(test_case.url);
    ASSERT_TRUE(url.is_valid());
    params.url = &url;
    params.first_party_origin = url::Origin::Create(google_url);
    params.is_third_party = true;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.parent_routing_id =
        web_contents->GetPrimaryMainFrame()->GetGlobalId();

    EXPECT_EQ(
        test_case.expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));
  }
}

// Tests disable rules with simple blocking rules.
TEST_F(RulesetMatcherTest, SetDisabledRuleIds) {
  TestRule rule_1 = CreateGenericRule(kMinValidID);
  rule_1.condition->url_filter = std::string("google.com");
  GURL google_url("http://google.com");

  TestRule rule_2 = CreateGenericRule(kMinValidID + 1);
  rule_2.condition->url_filter = std::string("yahoo.com");
  GURL yahoo_url("http://yahoo.com");

  GURL example_url("http://example.com");

  auto should_block_request = [](const RulesetMatcher& matcher,
                                 const RequestParams& params) {
    auto action =
        matcher.GetAction(params, RulesetMatchingStage::kOnBeforeRequest);
    return action.has_value() && action->IsBlockOrCollapse();
  };

  RequestParams params;
  params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  params.is_third_party = true;

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1, rule_2}, CreateTemporarySource(),
                                    &matcher));
  ASSERT_TRUE(matcher);

  params.url = &google_url;
  EXPECT_TRUE(should_block_request(*matcher, params));

  params.url = &yahoo_url;
  EXPECT_TRUE(should_block_request(*matcher, params));

  params.url = &example_url;
  EXPECT_FALSE(should_block_request(*matcher, params));

  EXPECT_THAT(matcher->GetDisabledRuleIdsForTesting(), testing::IsEmpty());

  matcher->SetDisabledRuleIds({*rule_1.id});

  EXPECT_THAT(matcher->GetDisabledRuleIdsForTesting(),
              testing::ElementsAreArray({*rule_1.id}));

  params.url = &google_url;
  EXPECT_FALSE(should_block_request(*matcher, params));

  params.url = &yahoo_url;
  EXPECT_TRUE(should_block_request(*matcher, params));

  params.url = &example_url;
  EXPECT_FALSE(should_block_request(*matcher, params));
}

class RulesetMatcherResponseHeadersTest : public RulesetMatcherTest {
 public:
  static constexpr char kExtensionId[] = "extensionid";

  RulesetMatcherResponseHeadersTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kDeclarativeNetRequestResponseHeaderMatching);
  }

 private:
  // TODO(crbug.com/40727004): Once feature is launched to stable and feature
  // flag can be removed, replace usages of this test class with just
  // DeclarativeNetRequestBrowserTest.
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCurrentChannel current_channel_override_{version_info::Channel::DEV};
};

// Test that GetOnHeadersReceivedAction only matches rules with response header
// conditions.
TEST_F(RulesetMatcherResponseHeadersTest, OnHeadersReceivedAction) {
  // Create 2 rules: a block rule that is matched in onBeforeRequest and a
  // redirect rule that is matched in onHeadersReceived.
  TestRule before_request_rule = CreateGenericRule(kMinValidID);
  before_request_rule.condition->url_filter = std::string("google.com");

  std::vector<TestHeaderCondition> header_condition(
      {TestHeaderCondition("key1", {"value1"}, {})});
  TestRule response_headers_rule = CreateGenericRule(kMinValidID + 1);
  response_headers_rule.condition->url_filter = std::string("google.com");
  response_headers_rule.condition->response_headers =
      std::move(header_condition);
  response_headers_rule.action->type = std::string("redirect");
  response_headers_rule.action->redirect.emplace();
  response_headers_rule.action->redirect->url = std::string("http://yahoo.com");

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  GURL google_url("http://google.com");
  RequestParams params =
      CreateRequestWithResponseHeaders(google_url, base_headers.get());
  // `response_headers_rule` matches on a header value, which requires host
  // access.
  params.has_host_access[kExtensionId] = true;

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(
      CreateVerifiedMatcher({before_request_rule, response_headers_rule},
                            CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);

  EXPECT_EQ(2u, matcher->GetRulesCount());
  EXPECT_EQ(1u, matcher->GetRulesCount(RulesetMatchingStage::kOnBeforeRequest));
  EXPECT_EQ(1u,
            matcher->GetRulesCount(RulesetMatchingStage::kOnHeadersReceived));

  // The request should be blocked if matched with `before_request_rule`.
  RequestAction expected_before_request_action =
      CreateRequestActionForTesting(RequestAction::Type::COLLAPSE, kMinValidID);
  EXPECT_EQ(expected_before_request_action,
            matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));

  // The request should be redirected if matched with `response_headers_rule`.
  RequestAction expected_headers_received_action =
      CreateRequestActionForTesting(RequestAction::Type::REDIRECT,
                                    kMinValidID + 1);
  expected_headers_received_action.redirect_url.emplace("http://yahoo.com");
  EXPECT_EQ(
      expected_headers_received_action,
      matcher->GetAction(params, RulesetMatchingStage::kOnHeadersReceived));

  // Check that disabling rules works for response header rules as well.
  EXPECT_THAT(matcher->GetDisabledRuleIdsForTesting(), testing::IsEmpty());
  matcher->SetDisabledRuleIds({*response_headers_rule.id});
  EXPECT_THAT(matcher->GetDisabledRuleIdsForTesting(),
              testing::ElementsAreArray({*response_headers_rule.id}));

  EXPECT_EQ(
      std::nullopt,
      matcher->GetAction(params, RulesetMatchingStage::kOnHeadersReceived));
}

// Test matching response header conditions with regex rules.
TEST_F(RulesetMatcherResponseHeadersTest, OnHeadersReceivedAction_Regex) {
  auto create_regex_rule = [](size_t id, const std::string& regex_filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = regex_filter;
    return rule;
  };

  // Create 3 rules:
  // - A regex allow rule which is matched in the onBeforeRequest stage.
  // - A url rule with a higher priority that redirects the request to
  //   urlmatch.com when matched in the onHeadersReceived stage.
  // - A regex rule that redirects the request to regexmatch.com when matched in
  //   the onHeadersReceived stage.

  TestRule before_request_rule =
      create_regex_rule(kMinValidID, R"(^(?:http|https)://[a-z\.]+\.com)");
  before_request_rule.action->type = std::string("allow");

  TestRule url_response_headers_rule = CreateGenericRule(kMinValidID + 1);
  url_response_headers_rule.priority = kMinValidPriority + 2;
  url_response_headers_rule.condition->url_filter = std::string("google.com");
  url_response_headers_rule.condition->response_headers = {
      TestHeaderCondition("key1", {}, {})};
  url_response_headers_rule.action->type = std::string("redirect");
  url_response_headers_rule.action->redirect.emplace();
  url_response_headers_rule.action->redirect->url =
      std::string("http://urlmatch.com");

  TestRule regex_response_headers_rule =
      create_regex_rule(kMinValidID + 2, R"(^(?:http|https)://[a-z\.]+\.com)");
  regex_response_headers_rule.priority = kMinValidPriority + 1;
  regex_response_headers_rule.condition->response_headers = {
      TestHeaderCondition("key1", {}, {})};
  regex_response_headers_rule.action->type = std::string("redirect");
  regex_response_headers_rule.action->redirect.emplace();
  regex_response_headers_rule.action->redirect->url =
      std::string("http://regexmatch.com");

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n";
  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base_headers_string));

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(
      CreateVerifiedMatcher({before_request_rule, url_response_headers_rule,
                             regex_response_headers_rule},
                            CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);

  struct Cases {
    std::string url;
    std::optional<RequestAction> expected_before_request_action;
    std::optional<RequestAction> expected_headers_received_action;
    std::optional<std::string> expected_redirect_url;
  };
  auto cases = std::to_array<Cases>({
      // The request to google.com will match `before_request_rule` for
      // GetBeforeRequestAction and `url_response_headers_rule` because of its
      // higher priority for GetOnHeadersReceivedAction.
      {"http://google.com",
       CreateRequestActionForTesting(RequestAction::Type::ALLOW, kMinValidID),
       CreateRequestActionForTesting(RequestAction::Type::REDIRECT,
                                     kMinValidID + 1, kMinValidPriority + 2),
       std::string("http://urlmatch.com")},

      // The request to someotherurl.com will match `before_request_rule` for
      // GetBeforeRequestAction and `regex_response_headers_rule` for
      // GetOnHeadersReceivedAction.
      {"http://someotherurl.com",
       CreateRequestActionForTesting(RequestAction::Type::ALLOW, kMinValidID),
       CreateRequestActionForTesting(RequestAction::Type::REDIRECT,
                                     kMinValidID + 2, kMinValidPriority + 1),
       std::string("http://regexmatch.com")},
  });

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    auto& test_case = cases[i];

    GURL url(test_case.url);
    ASSERT_TRUE(url.is_valid());
    RequestParams params =
        CreateRequestWithResponseHeaders(url, base_headers.get());

    EXPECT_EQ(
        test_case.expected_before_request_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnBeforeRequest));

    // We assign the value of redirect_url from the test case so that we can use
    // the operator== on the RequestAction below.
    if (test_case.expected_headers_received_action) {
      test_case.expected_headers_received_action->redirect_url =
          GURL(*test_case.expected_redirect_url);
    }

    EXPECT_EQ(
        test_case.expected_headers_received_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnHeadersReceived));
  }
}

// Test matching rules based on response header conditions.
TEST_F(RulesetMatcherResponseHeadersTest, MatchOnResponseHeaders) {
  std::vector<TestHeaderCondition> header_condition(
      {TestHeaderCondition("key1", {}, {}),
       TestHeaderCondition("key2", {"Value1", "value2"}, {"excludedValue"})});

  // `rule_1` will match if:
  //   - the key1 header is present, or:
  //   - the key2 header is present and has either value1 or value2, but not
  //     excludedValue
  // `rule_1` will fail to match if:
  //   - the excludedKey header is present
  TestRule rule_1 = CreateGenericRule(kMinValidID);
  rule_1.action->type = std::string("block");
  rule_1.condition->url_filter = std::string("google.com");
  rule_1.condition->response_headers = std::move(header_condition);
  rule_1.condition->excluded_response_headers =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("excludedKey", {}, {})});

  // `rule_2` will fail to match if:
  //   - the key3 header is present and has excludedValue, or:
  //   - the key4 header is present and does NOT have allowlistedValue
  TestRule rule_2 = CreateGenericRule(kMinValidID + 1);
  rule_2.action->type = std::string("block");
  rule_2.condition->url_filter = std::string("example.com");
  rule_2.condition->excluded_response_headers =
      std::vector<TestHeaderCondition>(
          {TestHeaderCondition("key3", {"excludedValue"}, {}),
           TestHeaderCondition("key4", {}, {"allowlistedValue"})});

  // `rule_3` will match if
  //   - the content-type header specifies a PDF
  TestRule rule_3 = CreateGenericRule(kMinValidID + 2);
  rule_3.action->type = std::string("block");
  rule_3.condition->url_filter = std::string("nopdf.com");
  rule_3.condition->response_headers = std::vector<TestHeaderCondition>(
      {TestHeaderCondition("content-type", {"*application/pdf*"}, {})});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1, rule_2, rule_3},
                                    CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);

  struct Cases {
    std::string url;
    std::string response_headers;
    std::optional<RequestAction> expected_action;
  };
  auto cases = std::to_array<Cases>({
      // No match for a non-matching URL.
      {"http://nomatch.com", "HTTP/1.0 200 OK\r\nKey1: Value1\r\n",
       std::nullopt},

      // No match if no header conditions match.
      {"http://google.com", "HTTP/1.0 200 OK\r\nNonmatching: Value\r\n",
       std::nullopt},

      // Test matching the key1 header by name only.
      {"http://google.com", "HTTP/1.0 200 OK\r\nkey1: any\r\n",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID)},

      // Test matching the key2 header by its value (case-insensitive).
      {"http://google.com", "HTTP/1.0 200 OK\r\nkey2: VALUE1\r\n",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID)},

      // No match since key2's value does not match what's specified in
      // `rule_1`.
      {"http://google.com", "HTTP/1.0 200 OK\r\nkey2: wrongvalue\r\n",
       std::nullopt},

      // No match since key2's value is excluded by `rule_1`. Note that the
      // excluded value takes precedence over the included `value1`.
      {"http://google.com",
       "HTTP/1.0 200 OK\r\nkey2: value1\r\nkey2: excludedValue\r\n",
       std::nullopt},

      // Test that only one included header condition needs to match (key1) for
      // the rule to match, even though another header condition does not match
      // (key2).
      {"http://google.com",
       "HTTP/1.0 200 OK\r\nkey1: any\r\nkey2: excludedValue\r\n",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID)},

      // No match since a header that is excluded by `rule_1` exists.
      {"http://google.com",
       "HTTP/1.0 200 OK\r\nkey1: any\r\nexcludedKey: value\r\n", std::nullopt},

      // For the next 3 test cases, the request matches if it does NOT contain
      // a "key3: value3" header-value pair.
      {"http://example.com", "HTTP/1.0 200 OK\r\nkey3: othervalue\r\n",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID + 1)},

      {"http://example.com", "HTTP/1.0 200 OK\r\nkey3: excludedValue\r\n",
       std::nullopt},

      {"http://example.com", "HTTP/1.0 200 OK\r\notherkey: key3doesntexist\r\n",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID + 1)},

      // For the next 2 test cases, the request with key4 matches iff key4
      // contains the allowlisted value.
      {"http://example.com", "HTTP/1.0 200 OK\r\nkey4: randomValue\r\n",
       std::nullopt},

      {"http://example.com", "HTTP/1.0 200 OK\r\nkey4: allowlistedValue\r\n",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID + 1)},

      // Test wildcard support for header value matching.
      {"http://nopdf.com",
       "HTTP/1.0 200 OK\r\ncontent-type: application/pdf; charset=utf-8\r\n",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID + 2)},
  });

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    GURL url(cases[i].url);
    ASSERT_TRUE(url.is_valid());

    auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(cases[i].response_headers.c_str()));
    RequestParams params =
        CreateRequestWithResponseHeaders(url, base_headers.get());
    // Some of the rules match on header values, which requires host access.
    params.has_host_access[kExtensionId] = true;
    EXPECT_EQ(
        cases[i].expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnHeadersReceived));
  }
}

// Test that response headers hidden from extensions by
// ExtensionsAPIClient::ShouldHideResponseHeader are treated as absent when
// matching response header conditions.
TEST_F(RulesetMatcherResponseHeadersTest, HiddenResponseHeadersNotMatched) {
  class TestExtensionsAPIClient : public ExtensionsAPIClient {
   public:
    bool ShouldHideResponseHeader(
        const GURL& url,
        const std::string& header_name) const override {
      return url.host() == "hidden.com" &&
             base::EqualsCaseInsensitiveASCII(header_name, "secret");
    }
  };
  TestExtensionsAPIClient api_client;

  // `rule_1` matches if the secret header is present with a value starting
  // with "a" (i.e. a value-probing rule).
  TestRule rule_1 = CreateGenericRule(kMinValidID);
  rule_1.action->type = std::string("block");
  rule_1.condition->url_filter = std::string("|http://");
  rule_1.condition->response_headers = std::vector<TestHeaderCondition>(
      {TestHeaderCondition("secret", {"a*"}, {})});

  // `rule_2` does not match if the secret header is present.
  TestRule rule_2 = CreateGenericRule(kMinValidID + 1);
  rule_2.action->type = std::string("block");
  rule_2.condition->url_filter = std::string("|https://");
  rule_2.condition->excluded_response_headers =
      std::vector<TestHeaderCondition>({TestHeaderCondition("secret", {}, {})});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1, rule_2}, CreateTemporarySource(),
                                    &matcher));
  ASSERT_TRUE(matcher);

  struct Cases {
    std::string url;
    std::optional<RequestAction> expected_action;
  };
  auto cases = std::to_array<Cases>({
      // The secret header is visible for non-hidden hosts.
      {"http://visible.com", CreateRequestActionForTesting(
                                 RequestAction::Type::COLLAPSE, kMinValidID)},
      {"https://visible.com", std::nullopt},

      // The secret header is hidden for hidden.com, so `rule_1` does not match
      // and `rule_2` is not excluded.
      {"http://hidden.com", std::nullopt},
      {"https://hidden.com",
       CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                     kMinValidID + 1)},
  });

  auto base_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.0 200 OK\r\nSecret: abc\r\n"));
  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    GURL url(cases[i].url);
    ASSERT_TRUE(url.is_valid());

    RequestParams params =
        CreateRequestWithResponseHeaders(url, base_headers.get());
    params.has_host_access[kExtensionId] = true;
    EXPECT_EQ(
        cases[i].expected_action,
        matcher->GetAction(params, RulesetMatchingStage::kOnHeadersReceived));
  }
}

// Tests that rules which match on the values of response headers other than
// CORS-safelisted response headers and Content-Encoding are only matched if the
// extension has host access to the request, and that lower priority rules are
// matched instead otherwise.
TEST_F(RulesetMatcherResponseHeadersTest,
       HeaderValueMatchingRequiresHostAccess) {
  struct RuleData {
    int id;
    int priority;
    std::string action_type;
    std::string url_filter;
    std::optional<std::vector<TestHeaderCondition>> response_headers;
    std::optional<std::vector<TestHeaderCondition>> excluded_response_headers;
  };
  auto rule_data = std::to_array<RuleData>({
      // Matches on the value of the set-cookie header: requires host access.
      {1, 3, "allow", "secret.com",
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("set-cookie", {"sid=a*"}, {})}),
       std::nullopt},
      // Matches on the presence of the set-cookie header: does not require host
      // access.
      {2, 1, "block", "secret.com",
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("set-cookie", {}, {})}),
       std::nullopt},
      // Matches on the values of CORS-safelisted headers and Content-Encoding
      // (with header names in varying case): does not require host access.
      {3, 1, "block", "allowlisted.com",
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("Content-Type", {"*pdf*"}, {}),
            TestHeaderCondition("LAST-MODIFIED", {"*2026*"}, {}),
            TestHeaderCondition("content-length", {"5"}, {}),
            TestHeaderCondition("Content-Encoding", {"gzip*"}, {})}),
       std::nullopt},
      // Excludes on the value of a custom header: requires host access.
      {4, 1, "block", "excluded.com", std::nullopt,
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("x-secret", {"0"}, {})})},
      // Matches on the values of an allowlisted header and a non-allowlisted
      // header: requires host access.
      {5, 1, "block", "mixed.com",
       std::vector<TestHeaderCondition>(
           {TestHeaderCondition("content-type", {"*pdf*"}, {}),
            TestHeaderCondition("x-custom", {}, {"value"})}),
       std::nullopt},
  });

  std::vector<TestRule> rules;
  for (const auto& data : rule_data) {
    TestRule rule = CreateGenericRule(data.id);
    rule.priority = data.priority;
    rule.action->type = data.action_type;
    rule.condition->url_filter = data.url_filter;
    rule.condition->response_headers = data.response_headers;
    rule.condition->excluded_response_headers = data.excluded_response_headers;
    rules.push_back(std::move(rule));
  }

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(rules, CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);

  auto collapse_action = [&rule_data](size_t index) {
    return CreateRequestActionForTesting(RequestAction::Type::COLLAPSE,
                                         rule_data[index].id,
                                         rule_data[index].priority);
  };

  struct {
    std::string url;
    std::string response_headers;
    std::optional<RequestAction> expected_action_with_host_access;
    std::optional<RequestAction> expected_action_without_host_access;
  } cases[] = {
      // With host access, the outcome depends on the set-cookie value. Without
      // host access, rule 1 is never matched so the outcome is the same
      // regardless of the value: rule 2 is matched instead.
      {"http://secret.com", "HTTP/1.0 200 OK\r\nSet-Cookie: sid=abc\r\n",
       CreateRequestActionForTesting(RequestAction::Type::ALLOW,
                                     rule_data[0].id, rule_data[0].priority),
       collapse_action(1)},
      {"http://secret.com", "HTTP/1.0 200 OK\r\nSet-Cookie: sid=xyz\r\n",
       collapse_action(1), collapse_action(1)},

      // Allowlisted header values can be matched without host access.
      {"http://allowlisted.com",
       "HTTP/1.0 200 OK\r\nContent-Type: application/pdf\r\n",
       collapse_action(2), collapse_action(2)},

      // Excluded header values require host access too.
      {"http://excluded.com", "HTTP/1.0 200 OK\r\nX-Secret: 5\r\n",
       collapse_action(3), std::nullopt},

      // A rule requires host access if any of its header conditions does.
      {"http://mixed.com",
       "HTTP/1.0 200 OK\r\nContent-Type: application/pdf\r\n",
       collapse_action(4), std::nullopt},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing %s with headers %s",
                                    test_case.url.c_str(),
                                    test_case.response_headers.c_str()));
    GURL url(test_case.url);
    ASSERT_TRUE(url.is_valid());
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(test_case.response_headers));

    // Note: separate RequestParams are used since actions may be cached.
    RequestParams params_without_host_access =
        CreateRequestWithResponseHeaders(url, headers.get());
    EXPECT_FALSE(params_without_host_access.HasHostAccess(kExtensionId));
    EXPECT_EQ(test_case.expected_action_without_host_access,
              matcher->GetAction(params_without_host_access,
                                 RulesetMatchingStage::kOnHeadersReceived));

    RequestParams params_with_host_access =
        CreateRequestWithResponseHeaders(url, headers.get());
    params_with_host_access.has_host_access[kExtensionId] = true;
    EXPECT_EQ(test_case.expected_action_with_host_access,
              matcher->GetAction(params_with_host_access,
                                 RulesetMatchingStage::kOnHeadersReceived));
  }
}

// Same as above, but for regex rules.
TEST_F(RulesetMatcherResponseHeadersTest,
       HeaderValueMatchingRequiresHostAccess_Regex) {
  TestRule allow_rule = CreateGenericRule(kMinValidID);
  allow_rule.priority = kMinValidPriority + 1;
  allow_rule.action->type = std::string("allow");
  allow_rule.condition->url_filter.reset();
  allow_rule.condition->regex_filter = std::string(R"(^http://secret\.com)");
  allow_rule.condition->response_headers = std::vector<TestHeaderCondition>(
      {TestHeaderCondition("set-cookie", {"sid=a*"}, {})});

  TestRule block_rule = CreateGenericRule(kMinValidID + 1);
  block_rule.priority = kMinValidPriority;
  block_rule.condition->url_filter.reset();
  block_rule.condition->regex_filter = std::string(R"(^http://secret\.com)");
  block_rule.condition->response_headers = std::vector<TestHeaderCondition>(
      {TestHeaderCondition("set-cookie", {}, {})});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({allow_rule, block_rule},
                                    CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);

  GURL url("http://secret.com");
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.0 200 OK\r\nSet-Cookie: sid=abc\r\n"));

  RequestAction expected_block_action = CreateRequestActionForTesting(
      RequestAction::Type::COLLAPSE, *block_rule.id, *block_rule.priority);
  RequestAction expected_allow_action = CreateRequestActionForTesting(
      RequestAction::Type::ALLOW, *allow_rule.id, *allow_rule.priority);

  RequestParams params_without_host_access =
      CreateRequestWithResponseHeaders(url, headers.get());
  params_without_host_access.has_host_access[kExtensionId] = false;
  EXPECT_EQ(expected_block_action,
            matcher->GetAction(params_without_host_access,
                               RulesetMatchingStage::kOnHeadersReceived));

  RequestParams params_with_host_access =
      CreateRequestWithResponseHeaders(url, headers.get());
  params_with_host_access.has_host_access[kExtensionId] = true;
  EXPECT_EQ(expected_allow_action,
            matcher->GetAction(params_with_host_access,
                               RulesetMatchingStage::kOnHeadersReceived));
}

// Tests that rules matching on response header values can be matched without
// host access if the restriction is disabled via its kill switch.
TEST_F(RulesetMatcherResponseHeadersTest,
       HeaderValueMatchingRestrictionDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::
          kDeclarativeNetRequestRestrictResponseHeaderValueMatching);

  TestRule rule = CreateGenericRule(kMinValidID);
  rule.condition->url_filter = std::string("secret.com");
  rule.condition->response_headers = std::vector<TestHeaderCondition>(
      {TestHeaderCondition("set-cookie", {"sid=a*"}, {})});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);

  GURL url("http://secret.com");
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.0 200 OK\r\nSet-Cookie: sid=abc\r\n"));
  RequestParams params = CreateRequestWithResponseHeaders(url, headers.get());
  ASSERT_FALSE(params.HasHostAccess(kExtensionId));
  EXPECT_EQ(
      CreateRequestActionForTesting(RequestAction::Type::COLLAPSE, *rule.id),
      matcher->GetAction(params, RulesetMatchingStage::kOnHeadersReceived));
}

// Tests that allowAllRequests rules which match on response header values
// requiring host access only allowlist a frame on navigation if the extension
// has host access to it.
TEST_F(RulesetMatcherResponseHeadersTest,
       HeaderValueMatchingRequiresHostAccess_AllowlistedFrames) {
  TestRule rule = CreateGenericRule(kMinValidID);
  rule.action->type = std::string("allowAllRequests");
  rule.condition->url_filter = std::string("secret.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.condition->response_headers = std::vector<TestHeaderCondition>(
      {TestHeaderCondition("set-cookie", {"sid=a*"}, {})});

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher({rule}, CreateTemporarySource(), &matcher));
  ASSERT_TRUE(matcher);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          browser_context(), content::SiteInstance::Create(browser_context()));
  ASSERT_TRUE(web_contents);

  GURL url("http://secret.com");
  content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(url);
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  testing::NiceMock<content::MockNavigationHandle> navigation_handle(
      url, main_frame);
  navigation_handle.set_has_committed(true);
  navigation_handle.set_response_headers(
      base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(
              "HTTP/1.0 200 OK\r\nSet-Cookie: sid=abc\r\n")));

  matcher->OnDidFinishNavigation(&navigation_handle,
                                 /*has_host_access_for_navigation=*/false);
  EXPECT_EQ(std::nullopt,
            matcher->GetAllowlistedFrameActionForTesting(main_frame));

  matcher->OnDidFinishNavigation(&navigation_handle,
                                 /*has_host_access_for_navigation=*/true);
  EXPECT_EQ(CreateRequestActionForTesting(
                RequestAction::Type::ALLOW_ALL_REQUESTS, *rule.id),
            matcher->GetAllowlistedFrameActionForTesting(main_frame));
}

}  // namespace
}  // namespace extensions::declarative_net_request

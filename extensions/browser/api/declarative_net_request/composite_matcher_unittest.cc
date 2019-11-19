// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <string>
#include <utility>
#include <vector>

#include "components/version_info/version_info.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

using PageAccess = PermissionsData::PageAccess;
using RedirectActionInfo = CompositeMatcher::RedirectActionInfo;

namespace dnr_api = api::declarative_net_request;

class CompositeMatcherTest : public ::testing::Test {
 public:
  CompositeMatcherTest() : channel_(::version_info::Channel::UNKNOWN) {}

 private:
  // Run this on the trunk channel to ensure the API is available.
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcherTest);
};

// Ensure CompositeMatcher respects priority of individual rulesets.
TEST_F(CompositeMatcherTest, RulesetPriority) {
  TestRule block_rule = CreateGenericRule();
  block_rule.condition->url_filter = std::string("google.com");
  block_rule.id = kMinValidID;

  TestRule redirect_rule_1 = CreateGenericRule();
  redirect_rule_1.condition->url_filter = std::string("example.com");
  redirect_rule_1.priority = kMinValidPriority;
  redirect_rule_1.action->type = std::string("redirect");
  redirect_rule_1.action->redirect.emplace();
  redirect_rule_1.action->redirect->url = std::string("http://ruleset1.com");
  redirect_rule_1.id = kMinValidID + 1;

  // Create the first ruleset matcher.
  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule, redirect_rule_1},
      CreateTemporarySource(kSource1ID, kSource1Priority), &matcher_1));

  // Now create a second ruleset matcher.
  const size_t kSource2ID = 2;
  const size_t kSource2Priority = 2;
  TestRule allow_rule = block_rule;
  allow_rule.action->type = std::string("allow");
  TestRule redirect_rule_2 = redirect_rule_1;
  redirect_rule_2.action->redirect.emplace();
  redirect_rule_2.action->redirect->url = std::string("http://ruleset2.com");
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule, redirect_rule_2},
      CreateTemporarySource(kSource2ID, kSource2Priority), &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), nullptr /* action_tracker */);

  GURL google_url = GURL("http://google.com");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // The second ruleset should get more priority.
  EXPECT_FALSE(
      composite_matcher->GetBlockOrCollapseAction(google_params).has_value());

  GURL example_url = GURL("http://example.com");
  RequestParams example_params;
  example_params.url = &example_url;
  example_params.element_type =
      url_pattern_index::flat::ElementType_SUBDOCUMENT;
  example_params.is_third_party = false;

  RedirectActionInfo action_info = composite_matcher->GetRedirectAction(
      example_params, PageAccess::kAllowed);
  ASSERT_TRUE(action_info.action);
  EXPECT_EQ(GURL("http://ruleset2.com"), action_info.action->redirect_url);
  EXPECT_FALSE(action_info.notify_request_withheld);

  // Now switch the priority of the two rulesets. This requires re-constructing
  // the two ruleset matchers.
  matcher_1.reset();
  matcher_2.reset();
  matchers.clear();
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule, redirect_rule_1},
      CreateTemporarySource(kSource1ID, kSource2Priority), &matcher_1));
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule, redirect_rule_2},
      CreateTemporarySource(kSource2ID, kSource1Priority), &matcher_2));
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), nullptr /* action_tracker */);

  // Reusing request params means that their allow_rule_caches must be cleared.
  google_params.allow_rule_cache.clear();
  example_params.allow_rule_cache.clear();

  // The first ruleset should get more priority.
  EXPECT_TRUE(
      composite_matcher->GetBlockOrCollapseAction(google_params).has_value());

  action_info = composite_matcher->GetRedirectAction(example_params,
                                                     PageAccess::kAllowed);
  ASSERT_TRUE(action_info.action);
  EXPECT_EQ(GURL("http://ruleset1.com"), action_info.action->redirect_url);
  EXPECT_FALSE(action_info.notify_request_withheld);
}

// Ensure allow rules in a higher priority matcher override redirect
// and removeHeader rules from lower priority matchers.
TEST_F(CompositeMatcherTest, AllowRuleOverrides) {
  TestRule allow_rule_1 = CreateGenericRule();
  allow_rule_1.id = kMinValidID;
  allow_rule_1.condition->url_filter = std::string("google.com");
  allow_rule_1.action->type = std::string("allow");

  TestRule remove_headers_rule_1 = CreateGenericRule();
  remove_headers_rule_1.id = kMinValidID + 1;
  remove_headers_rule_1.condition->url_filter = std::string("example.com");
  remove_headers_rule_1.action->type = std::string("removeHeaders");
  remove_headers_rule_1.action->remove_headers_list =
      std::vector<std::string>({"referer", "setCookie"});

  // Create the first ruleset matcher, which allows requests to google.com and
  // removes headers from requests to example.com.
  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule_1, remove_headers_rule_1},
      CreateTemporarySource(kSource1ID, kSource1Priority,
                            dnr_api::SOURCE_TYPE_MANIFEST),
      &matcher_1));

  // Now set up rules and the second matcher.
  TestRule allow_rule_2 = allow_rule_1;
  allow_rule_2.condition->url_filter = std::string("example.com");

  TestRule redirect_rule_2 = CreateGenericRule();
  redirect_rule_2.condition->url_filter = std::string("google.com");
  redirect_rule_2.priority = kMinValidPriority;
  redirect_rule_2.action->type = std::string("redirect");
  redirect_rule_2.action->redirect.emplace();
  redirect_rule_2.action->redirect->url = std::string("http://ruleset2.com");
  redirect_rule_2.id = kMinValidID + 1;

  // Create a second ruleset matcher, which allows requests to example.com and
  // redirects requests to google.com.
  const size_t kSource2ID = 2;
  const size_t kSource2Priority = 2;
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(
      CreateVerifiedMatcher({allow_rule_2, redirect_rule_2},
                            CreateTemporarySource(kSource2ID, kSource2Priority,
                                                  dnr_api::SOURCE_TYPE_DYNAMIC),
                            &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), nullptr /* action_tracker */);

  // Send a request to google.com which should be redirected.
  GURL google_url = GURL("http://google.com");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // The second ruleset should get more priority.
  RedirectActionInfo action_info =
      composite_matcher->GetRedirectAction(google_params, PageAccess::kAllowed);
  ASSERT_TRUE(action_info.action);
  EXPECT_EQ(GURL("http://ruleset2.com"), action_info.action->redirect_url);
  EXPECT_FALSE(action_info.notify_request_withheld);

  // Send a request to example.com with headers, expect the allow rule to be
  // matched and the headers to remain.
  GURL example_url = GURL("http://example.com");
  RequestParams example_params;
  example_params.url = &example_url;
  example_params.element_type =
      url_pattern_index::flat::ElementType_SUBDOCUMENT;
  example_params.is_third_party = false;

  // Expect no headers to be removed.
  std::vector<RequestAction> remove_header_actions;
  EXPECT_EQ(0u, composite_matcher->GetRemoveHeadersMask(
                    example_params, 0u, &remove_header_actions));
  EXPECT_TRUE(remove_header_actions.empty());

  remove_header_actions.clear();

  // Now switch the priority of the two rulesets. This requires re-constructing
  // the two ruleset matchers.
  matcher_1.reset();
  matcher_2.reset();
  matchers.clear();
  ASSERT_TRUE(
      CreateVerifiedMatcher({allow_rule_1, remove_headers_rule_1},
                            CreateTemporarySource(kSource1ID, kSource2Priority,
                                                  dnr_api::SOURCE_TYPE_DYNAMIC),
                            &matcher_1));
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule_2, redirect_rule_2},
      CreateTemporarySource(kSource2ID, kSource1Priority,
                            dnr_api::SOURCE_TYPE_MANIFEST),
      &matcher_2));
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), nullptr /* action_tracker */);

  // Reusing request params means that their allow_rule_caches must be cleared.
  google_params.allow_rule_cache.clear();
  example_params.allow_rule_cache.clear();

  // The first ruleset should get more priority and so the request to google.com
  // should not be redirected.
  action_info =
      composite_matcher->GetRedirectAction(google_params, PageAccess::kAllowed);
  EXPECT_FALSE(action_info.action.has_value());
  EXPECT_FALSE(action_info.notify_request_withheld);

  // The request to example.com should now have its headers removed.
  example_params.allow_rule_cache.clear();
  uint8_t expected_mask =
      flat::RemoveHeaderType_referer | flat::RemoveHeaderType_set_cookie;
  EXPECT_EQ(expected_mask, composite_matcher->GetRemoveHeadersMask(
                               example_params, 0u, &remove_header_actions));
  ASSERT_EQ(1u, remove_header_actions.size());

  RequestAction expected_action = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *remove_headers_rule_1.id,
      kDefaultPriority, dnr_api::SOURCE_TYPE_DYNAMIC);
  expected_action.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kReferer);
  expected_action.response_headers_to_remove.push_back("set-cookie");
  EXPECT_EQ(expected_action, remove_header_actions[0]);
}

// Tests that header masks are correctly attributed to rules for multiple
// matchers in a CompositeMatcher.
TEST_F(CompositeMatcherTest, HeadersMaskForRules) {
  auto create_remove_headers_rule =
      [](int id, const std::string& url_filter,
         const std::vector<std::string>& remove_headers_list) {
        TestRule rule = CreateGenericRule();
        rule.id = id;
        rule.condition->url_filter = url_filter;
        rule.action->type = std::string("removeHeaders");
        rule.action->remove_headers_list = remove_headers_list;

        return rule;
      };

  TestRule static_rule_1 = create_remove_headers_rule(
      kMinValidID, "g*", std::vector<std::string>({"referer", "cookie"}));

  TestRule static_rule_2 = create_remove_headers_rule(
      kMinValidID + 1, "g*", std::vector<std::string>({"setCookie"}));

  TestRule dynamic_rule_1 = create_remove_headers_rule(
      kMinValidID, "google.com", std::vector<std::string>({"referer"}));

  TestRule dynamic_rule_2 = create_remove_headers_rule(
      kMinValidID + 2, "google.com", std::vector<std::string>({"setCookie"}));

  // Create the first ruleset matcher, which matches all requests with "g" in
  // their URL.
  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {static_rule_1, static_rule_2},
      CreateTemporarySource(kSource1ID, kSource1Priority,
                            dnr_api::SOURCE_TYPE_MANIFEST),
      &matcher_1));

  // Create a second ruleset matcher, which matches all requests from
  // |google.com|.
  const size_t kSource2ID = 2;
  const size_t kSource2Priority = 2;
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(
      CreateVerifiedMatcher({dynamic_rule_1, dynamic_rule_2},
                            CreateTemporarySource(kSource2ID, kSource2Priority,
                                                  dnr_api::SOURCE_TYPE_DYNAMIC),
                            &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), nullptr /* action_tracker */);

  GURL google_url = GURL("http://google.com");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  const uint8_t expected_mask = flat::RemoveHeaderType_referer |
                                flat::RemoveHeaderType_cookie |
                                flat::RemoveHeaderType_set_cookie;

  std::vector<RequestAction> actions;
  EXPECT_EQ(expected_mask, composite_matcher->GetRemoveHeadersMask(
                               google_params, 0u, &actions));

  // Construct expected request actions to be taken for a request to google.com.
  // Static actions are attributed to |matcher_1| and dynamic actions are
  // attributed to |matcher_2|.
  RequestAction static_action_1 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *static_rule_1.id, kDefaultPriority,
      dnr_api::SOURCE_TYPE_MANIFEST);
  static_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kCookie);

  RequestAction dynamic_action_1 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *dynamic_rule_1.id, kDefaultPriority,
      dnr_api::SOURCE_TYPE_DYNAMIC);
  dynamic_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kReferer);

  RequestAction dynamic_action_2 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *dynamic_rule_2.id, kDefaultPriority,
      dnr_api::SOURCE_TYPE_DYNAMIC);
  dynamic_action_2.response_headers_to_remove.push_back("set-cookie");

  EXPECT_THAT(actions, ::testing::UnorderedElementsAre(
                           ::testing::Eq(::testing::ByRef(static_action_1)),
                           ::testing::Eq(::testing::ByRef(dynamic_action_1)),
                           ::testing::Eq(::testing::ByRef(dynamic_action_2))));

  GURL gmail_url = GURL("http://gmail.com");
  RequestParams gmail_params;
  gmail_params.url = &gmail_url;
  gmail_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  gmail_params.is_third_party = false;

  actions.clear();
  EXPECT_EQ(expected_mask, composite_matcher->GetRemoveHeadersMask(
                               gmail_params, 0u, &actions));

  static_action_1 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *static_rule_1.id,
      dnr_api::SOURCE_TYPE_MANIFEST);
  static_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kCookie);
  static_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kReferer);

  RequestAction static_action_2 = CreateRequestActionForTesting(
      RequestAction::Type::REMOVE_HEADERS, *static_rule_2.id, kDefaultPriority,
      dnr_api::SOURCE_TYPE_MANIFEST);
  static_action_2.response_headers_to_remove.push_back("set-cookie");

  EXPECT_THAT(actions, ::testing::UnorderedElementsAre(
                           ::testing::Eq(::testing::ByRef(static_action_1)),
                           ::testing::Eq(::testing::ByRef(static_action_2))));
}

// Ensure CompositeMatcher detects requests to be notified based on the rule
// matched and whether the extenion has access to the request.
TEST_F(CompositeMatcherTest, NotifyWithholdFromPageAccess) {
  TestRule redirect_rule = CreateGenericRule();
  redirect_rule.condition->url_filter = std::string("google.com");
  redirect_rule.priority = kMinValidPriority;
  redirect_rule.action->type = std::string("redirect");
  redirect_rule.action->redirect.emplace();
  redirect_rule.action->redirect->url = std::string("http://ruleset1.com");
  redirect_rule.id = kMinValidID;

  TestRule upgrade_rule = CreateGenericRule();
  upgrade_rule.condition->url_filter = std::string("example.com");
  upgrade_rule.priority = kMinValidPriority + 1;
  upgrade_rule.action->type = std::string("upgradeScheme");
  upgrade_rule.id = kMinValidID + 1;

  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {redirect_rule, upgrade_rule},
      CreateTemporarySource(kSource1ID, kSource1Priority), &matcher_1));

  // Create a composite matcher.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), nullptr /* action_tracker */);

  GURL google_url = GURL("http://google.com");
  GURL example_url = GURL("http://example.com");
  GURL yahoo_url = GURL("http://yahoo.com");

  GURL ruleset1_url = GURL("http://ruleset1.com");
  GURL https_example_url = GURL("https://example.com");

  struct {
    GURL& request_url;
    PageAccess access;
    base::Optional<GURL> expected_final_url;
    bool should_notify_withheld;
  } test_cases[] = {
      // If access to the request is allowed, we should not notify that
      // the request is withheld.
      {google_url, PageAccess::kAllowed, ruleset1_url, false},
      {example_url, PageAccess::kAllowed, https_example_url, false},
      {yahoo_url, PageAccess::kAllowed, base::nullopt, false},

      // Notify the request is withheld if it matches with a redirect rule.
      {google_url, PageAccess::kWithheld, base::nullopt, true},
      // If the page access to the request is withheld but it matches with
      // an upgrade rule, or no rule, then we should not notify.
      {example_url, PageAccess::kWithheld, https_example_url, false},
      {yahoo_url, PageAccess::kWithheld, base::nullopt, false},

      // If access to the request is denied instead of withheld, the extension
      // should not be notified.
      {google_url, PageAccess::kDenied, base::nullopt, false},
      // If the page access to the request is denied but it matches with
      // an upgrade rule, or no rule, then we should not notify.
      {example_url, PageAccess::kDenied, https_example_url, false},
      {yahoo_url, PageAccess::kDenied, base::nullopt, false},
  };

  for (const auto& test_case : test_cases) {
    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    RedirectActionInfo redirect_action_info =
        composite_matcher->GetRedirectAction(params, test_case.access);

    EXPECT_EQ(test_case.should_notify_withheld,
              redirect_action_info.notify_request_withheld);
  }
}

// Tests that the redirect url within an extension's ruleset is chosen based on
// the highest priority matching rule.
TEST_F(CompositeMatcherTest, GetRedirectUrlFromPriority) {
  TestRule abc_redirect = CreateGenericRule();
  abc_redirect.condition->url_filter = std::string("*abc*");
  abc_redirect.priority = kMinValidPriority;
  abc_redirect.action->type = std::string("redirect");
  abc_redirect.action->redirect.emplace();
  abc_redirect.action->redirect->url = std::string("http://google.com");
  abc_redirect.id = kMinValidID;

  TestRule def_upgrade = CreateGenericRule();
  def_upgrade.condition->url_filter = std::string("*def*");
  def_upgrade.priority = kMinValidPriority + 1;
  def_upgrade.action->type = std::string("upgradeScheme");
  def_upgrade.id = kMinValidID + 1;

  TestRule ghi_redirect = CreateGenericRule();
  ghi_redirect.condition->url_filter = std::string("*ghi*");
  ghi_redirect.priority = kMinValidPriority + 2;
  ghi_redirect.action->type = std::string("redirect");
  ghi_redirect.action->redirect.emplace();
  ghi_redirect.action->redirect->url = std::string("http://example.com");
  ghi_redirect.id = kMinValidID + 2;

  // In terms of priority: ghi > def > abc.

  const size_t kSource1ID = 1;
  const size_t kSource1Priority = 1;
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {abc_redirect, def_upgrade, ghi_redirect},
      CreateTemporarySource(kSource1ID, kSource1Priority), &matcher_1));

  // Create a composite matcher.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), nullptr /* action_tracker */);

  struct {
    GURL request_url;
    base::Optional<GURL> expected_final_url;
  } test_cases[] = {
      // Test requests which match exactly one rule.
      {GURL("http://abc.com"), GURL("http://google.com")},
      {GURL("http://def.com"), GURL("https://def.com")},
      {GURL("http://ghi.com"), GURL("http://example.com")},

      // The upgrade rule has a higher priority than the redirect rule matched
      // so the request should be upgraded.
      {GURL("http://abcdef.com"), GURL("https://abcdef.com")},

      // The upgrade rule has a lower priority than the redirect rule matched so
      // the request should be redirected.
      {GURL("http://defghi.com"), GURL("http://example.com")},

      // The request should be redirected as it does not match the upgrade rule
      // because of its scheme.
      {GURL("https://abcdef.com"), GURL("http://google.com")},
      {GURL("http://xyz.com"), base::nullopt},
  };

  for (const auto& test_case : test_cases) {
    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    RedirectActionInfo redirect_action_info =
        composite_matcher->GetRedirectAction(params, PageAccess::kAllowed);

    if (test_case.expected_final_url) {
      ASSERT_TRUE(redirect_action_info.action);
      EXPECT_EQ(test_case.expected_final_url,
                redirect_action_info.action->redirect_url);
    } else {
      EXPECT_FALSE(redirect_action_info.action.has_value());
    }

    EXPECT_FALSE(redirect_action_info.notify_request_withheld);
  }
}

}  // namespace declarative_net_request
}  // namespace extensions

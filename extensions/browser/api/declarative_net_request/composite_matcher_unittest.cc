// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {
namespace {

using PageAccess = PermissionsData::PageAccess;
using ActionInfo = CompositeMatcher::ActionInfo;

namespace dnr_api = api::declarative_net_request;

using CompositeMatcherTest = ::testing::Test;

TestRule CreateModifyHeadersRule(
    int id,
    int priority,
    absl::optional<std::string> url_filter,
    absl::optional<std::string> regex_filter,
    absl::optional<std::vector<TestHeaderInfo>> request_headers_list,
    absl::optional<std::vector<TestHeaderInfo>> response_headers_list) {
  TestRule rule = CreateGenericRule();
  rule.id = id;
  rule.priority = priority;

  if (url_filter)
    rule.condition->url_filter = url_filter;
  else if (regex_filter) {
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = regex_filter;
  }

  rule.action->type = std::string("modifyHeaders");
  if (request_headers_list)
    rule.action->request_headers = std::move(request_headers_list);
  if (response_headers_list)
    rule.action->response_headers = std::move(response_headers_list);
  return rule;
}

// Ensure that the rules in a CompositeMatcher are in the same priority space.
TEST_F(CompositeMatcherTest, SamePrioritySpace) {
  // Create the first ruleset matcher. It allows requests to google.com.
  TestRule allow_rule = CreateGenericRule();
  allow_rule.id = kMinValidID;
  allow_rule.condition->url_filter = std::string("google.com");
  allow_rule.action->type = std::string("allow");
  allow_rule.priority = 1;
  std::unique_ptr<RulesetMatcher> allow_matcher;
  RulesetID ruleset_id_one(1);
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule}, CreateTemporarySource(ruleset_id_one), &allow_matcher));

  // Now create the second matcher. It blocks requests to google.com, with
  // higher priority than the allow rule.
  TestRule block_rule = allow_rule;
  block_rule.action->type = std::string("block");
  block_rule.priority = 2;
  std::unique_ptr<RulesetMatcher> block_matcher;
  RulesetID ruleset_id_two(2);
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule}, CreateTemporarySource(ruleset_id_two), &block_matcher));

  // Create a composite matcher with both rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(allow_matcher));
  matchers.push_back(std::move(block_matcher));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

  GURL google_url("http://google.com");
  RequestParams params;
  params.url = &google_url;

  // The block rule should be higher priority.
  ActionInfo action_info =
      composite_matcher->GetBeforeRequestAction(params, PageAccess::kAllowed);
  ASSERT_TRUE(action_info.action);
  EXPECT_EQ(action_info.action->type, RequestAction::Type::BLOCK);

  // Now swap the priority of the rules, which requires re-creating the ruleset
  // matchers and composite matcher.
  allow_rule.priority = 2;
  block_rule.priority = 1;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {allow_rule}, CreateTemporarySource(ruleset_id_one), &allow_matcher));
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule}, CreateTemporarySource(ruleset_id_two), &block_matcher));
  matchers.clear();
  matchers.push_back(std::move(allow_matcher));
  matchers.push_back(std::move(block_matcher));
  composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

  // The allow rule should now have higher priority.
  action_info =
      composite_matcher->GetBeforeRequestAction(params, PageAccess::kAllowed);
  ASSERT_TRUE(action_info.action);
  EXPECT_EQ(action_info.action->type, RequestAction::Type::ALLOW);
}

// Tests the GetModifyHeadersActions method.
TEST_F(CompositeMatcherTest, GetModifyHeadersActions) {
  TestRule rule_1 = CreateModifyHeadersRule(
      kMinValidID, kMinValidPriority, "google.com", absl::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header1", "remove", absl::nullopt),
           TestHeaderInfo("header2", "set", "value2")}),
      absl::nullopt);

  TestRule rule_2 = CreateModifyHeadersRule(
      kMinValidID, kMinValidPriority + 1, "/path", absl::nullopt, absl::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header1", "remove", absl::nullopt),
           TestHeaderInfo("header2", "append", "VALUE2"),
           TestHeaderInfo("header3", "set", "VALUE3")}));

  // Create the first ruleset matcher, which matches all requests from
  // |google.com|.
  const RulesetID kSource1ID(1);
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1}, CreateTemporarySource(kSource1ID),
                                    &matcher_1));

  // Create a second ruleset matcher, which matches all requests with |/path| in
  // their URL.
  const RulesetID kSource2ID(2);
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_2}, CreateTemporarySource(kSource2ID),
                                    &matcher_2));

  // Create a composite matcher with the two rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

  GURL google_url = GURL("http://google.com/path");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // Call GetBeforeRequestAction first to ensure that test and production code
  // paths are consistent.
  composite_matcher->GetBeforeRequestAction(google_params,
                                            PageAccess::kAllowed);

  std::vector<RequestAction> actions =
      composite_matcher->GetModifyHeadersActions(google_params);

  // Construct expected request actions to be taken for a request to google.com.
  RequestAction action_1 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_1.id, *rule_1.priority, kSource1ID);
  action_1.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE,
                                absl::nullopt),
      RequestAction::HeaderInfo("header2", dnr_api::HEADER_OPERATION_SET,
                                "value2")};

  RequestAction action_2 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_2.id, *rule_2.priority, kSource2ID);
  action_2.response_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE,
                                absl::nullopt),
      RequestAction::HeaderInfo("header2", dnr_api::HEADER_OPERATION_APPEND,
                                "VALUE2"),
      RequestAction::HeaderInfo("header3", dnr_api::HEADER_OPERATION_SET,
                                "VALUE3")};

  // |action_2| should be before |action_1| because |rule_2|
  // has a higher priority.
  EXPECT_THAT(actions, ::testing::ElementsAre(
                           ::testing::Eq(::testing::ByRef(action_2)),
                           ::testing::Eq(::testing::ByRef(action_1))));

  // Now swap the priority of the rules, which requires re-creating the ruleset
  // matchers and composite matcher.
  rule_1.priority = kMinValidPriority + 1;
  rule_2.priority = kMinValidPriority;
  ASSERT_TRUE(CreateVerifiedMatcher({rule_1}, CreateTemporarySource(kSource1ID),
                                    &matcher_1));
  ASSERT_TRUE(CreateVerifiedMatcher({rule_2}, CreateTemporarySource(kSource2ID),
                                    &matcher_2));

  matchers.clear();
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

  // Call GetBeforeRequestAction first to ensure that test and production code
  // paths are consistent.
  composite_matcher->GetBeforeRequestAction(google_params,
                                            PageAccess::kAllowed);

  // Re-create |action_1| and |action_2| with the updated rule
  // priorities. The headers modified by each action should not change.
  actions = composite_matcher->GetModifyHeadersActions(google_params);
  action_1 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_1.id, *rule_1.priority, kSource1ID);
  action_1.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE,
                                absl::nullopt),
      RequestAction::HeaderInfo("header2", dnr_api::HEADER_OPERATION_SET,
                                "value2")};

  action_2 =
      CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                    *rule_2.id, *rule_2.priority, kSource2ID);
  action_2.response_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE,
                                absl::nullopt),
      RequestAction::HeaderInfo("header2", dnr_api::HEADER_OPERATION_APPEND,
                                "VALUE2"),
      RequestAction::HeaderInfo("header3", dnr_api::HEADER_OPERATION_SET,
                                "VALUE3")};

  // |action_1| should now be before |action_2| after their
  // priorities have been reversed.
  EXPECT_THAT(actions, ::testing::ElementsAre(
                           ::testing::Eq(::testing::ByRef(action_1)),
                           ::testing::Eq(::testing::ByRef(action_2))));
}

// Tests that GetModifyHeadersActions method omits rules with an equal or lower
// priority than a matched allow or allowAllRequests rule.
TEST_F(CompositeMatcherTest, GetModifyHeadersActions_Priority) {
  using HeaderInfo = RequestAction::HeaderInfo;
  int allow_rule_priority = kMinValidPriority + 1;

  TestRule allow_rule = CreateGenericRule();
  allow_rule.id = kMinValidID;
  allow_rule.condition->url_filter = std::string("google.com/1");
  allow_rule.action->type = std::string("allow");
  allow_rule.priority = allow_rule_priority;

  TestRule url_rule_1 = CreateModifyHeadersRule(
      kMinValidID + 1, allow_rule_priority - 1, "google.com", absl::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header1", "remove", absl::nullopt)}),
      absl::nullopt);

  TestRule url_rule_2 = CreateModifyHeadersRule(
      kMinValidID + 2, allow_rule_priority, "google.com", absl::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header2", "remove", absl::nullopt)}),
      absl::nullopt);

  TestRule url_rule_3 = CreateModifyHeadersRule(
      kMinValidID + 3, allow_rule_priority + 1, "google.com", absl::nullopt,
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header3", "remove", absl::nullopt)}),
      absl::nullopt);

  TestRule regex_rule_1 = CreateModifyHeadersRule(
      kMinValidID + 4, allow_rule_priority - 1, absl::nullopt, R"(google\.com)",
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header4", "remove", absl::nullopt)}),
      absl::nullopt);

  TestRule regex_rule_2 = CreateModifyHeadersRule(
      kMinValidID + 5, allow_rule_priority, absl::nullopt, R"(google\.com)",
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header5", "remove", absl::nullopt)}),
      absl::nullopt);

  TestRule regex_rule_3 = CreateModifyHeadersRule(
      kMinValidID + 6, allow_rule_priority + 1, absl::nullopt, R"(google\.com)",
      std::vector<TestHeaderInfo>(
          {TestHeaderInfo("header6", "remove", absl::nullopt)}),
      absl::nullopt);

  const RulesetID kSource1ID(1);
  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(
      CreateVerifiedMatcher({allow_rule, url_rule_1, url_rule_2, url_rule_3},
                            CreateTemporarySource(kSource1ID), &matcher_1));

  const RulesetID kSource2ID(2);
  std::unique_ptr<RulesetMatcher> matcher_2;
  ASSERT_TRUE(CreateVerifiedMatcher({regex_rule_1, regex_rule_2, regex_rule_3},
                                    CreateTemporarySource(kSource2ID),
                                    &matcher_2));

  // Create a CompositeMatcher with the rulesets.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  matchers.push_back(std::move(matcher_2));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

  // Make a request to "http://google.com/1" which matches with all
  // modifyHeaders rules and |allow_rule|.
  GURL google_url = GURL("http://google.com/1");
  RequestParams google_params;
  google_params.url = &google_url;
  google_params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  google_params.is_third_party = false;

  // Call GetBeforeRequestAction first to ensure that test and production code
  // paths are consistent.
  composite_matcher->GetBeforeRequestAction(google_params,
                                            PageAccess::kAllowed);

  std::vector<RequestAction> actions =
      composite_matcher->GetModifyHeadersActions(google_params);

  auto create_action_for_rule =
      [](const TestRule& rule, const RulesetID& ruleset_id,
         const std::vector<HeaderInfo>& request_headers) {
        RequestAction action =
            CreateRequestActionForTesting(RequestAction::Type::MODIFY_HEADERS,
                                          *rule.id, *rule.priority, ruleset_id);

        action.request_headers_to_modify = request_headers;
        return action;
      };

  RequestAction header_3_action = create_action_for_rule(
      url_rule_3, kSource1ID,
      {HeaderInfo("header3", dnr_api::HEADER_OPERATION_REMOVE, absl::nullopt)});
  RequestAction header_6_action = create_action_for_rule(
      regex_rule_3, kSource2ID,
      {HeaderInfo("header6", dnr_api::HEADER_OPERATION_REMOVE, absl::nullopt)});

  // For the request to "http://google.com/1", since |url_rule_3| and
  // |regex_rule_3| are the only rules with a greater priority than
  // |allow_rule|, "header3" and "header4" should be removed.
  EXPECT_THAT(actions, ::testing::UnorderedElementsAre(
                           ::testing::Eq(::testing::ByRef(header_3_action)),
                           ::testing::Eq(::testing::ByRef(header_6_action))));

  // Make a request to "http://google.com/2" which should match with all
  // modifyHeaders rules but not |allow_rule|.
  google_url = GURL("http://google.com/2");
  google_params.url = &google_url;

  // Call GetBeforeRequestAction first to ensure that test and production code
  // paths are consistent.
  composite_matcher->GetBeforeRequestAction(google_params,
                                            PageAccess::kAllowed);
  actions = composite_matcher->GetModifyHeadersActions(google_params);

  RequestAction header_1_action = create_action_for_rule(
      url_rule_1, kSource1ID,
      {HeaderInfo("header1", dnr_api::HEADER_OPERATION_REMOVE, absl::nullopt)});
  RequestAction header_2_action = create_action_for_rule(
      url_rule_2, kSource1ID,
      {HeaderInfo("header2", dnr_api::HEADER_OPERATION_REMOVE, absl::nullopt)});
  RequestAction header_4_action = create_action_for_rule(
      regex_rule_1, kSource2ID,
      {HeaderInfo("header4", dnr_api::HEADER_OPERATION_REMOVE, absl::nullopt)});
  RequestAction header_5_action = create_action_for_rule(
      regex_rule_2, kSource2ID,
      {HeaderInfo("header5", dnr_api::HEADER_OPERATION_REMOVE, absl::nullopt)});

  // For the request to "http://google.com/2", "header1" to "header6" should be
  // removed since all modifyHeaders rules are matched and there is no matching
  // allow/allowAllRequests rule.
  EXPECT_THAT(actions, ::testing::UnorderedElementsAre(
                           ::testing::Eq(::testing::ByRef(header_1_action)),
                           ::testing::Eq(::testing::ByRef(header_2_action)),
                           ::testing::Eq(::testing::ByRef(header_3_action)),
                           ::testing::Eq(::testing::ByRef(header_4_action)),
                           ::testing::Eq(::testing::ByRef(header_5_action)),
                           ::testing::Eq(::testing::ByRef(header_6_action))));
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

  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher({redirect_rule, upgrade_rule},
                                    CreateTemporarySource(), &matcher_1));

  // Create a composite matcher.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

  GURL google_url = GURL("http://google.com");
  GURL example_url = GURL("http://example.com");
  GURL yahoo_url = GURL("http://yahoo.com");

  GURL ruleset1_url = GURL("http://ruleset1.com");
  GURL https_example_url = GURL("https://example.com");

  struct {
    GURL& request_url;
    PageAccess access;
    absl::optional<GURL> expected_final_url;
    bool should_notify_withheld;
  } test_cases[] = {
      // If access to the request is allowed, we should not notify that
      // the request is withheld.
      {google_url, PageAccess::kAllowed, ruleset1_url, false},
      {example_url, PageAccess::kAllowed, https_example_url, false},
      {yahoo_url, PageAccess::kAllowed, absl::nullopt, false},

      // Notify the request is withheld if it matches with a redirect rule.
      {google_url, PageAccess::kWithheld, absl::nullopt, true},
      // If the page access to the request is withheld but it matches with
      // an upgrade rule, or no rule, then we should not notify.
      {example_url, PageAccess::kWithheld, https_example_url, false},
      {yahoo_url, PageAccess::kWithheld, absl::nullopt, false},

      // If access to the request is denied instead of withheld, the extension
      // should not be notified.
      {google_url, PageAccess::kDenied, absl::nullopt, false},
      // If the page access to the request is denied but it matches with
      // an upgrade rule, or no rule, then we should not notify.
      {example_url, PageAccess::kDenied, https_example_url, false},
      {yahoo_url, PageAccess::kDenied, absl::nullopt, false},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "request_url=%s, access=%d, expected_final_url=%s, "
        "should_notify_withheld=%d",
        test_case.request_url.spec().c_str(), test_case.access,
        test_case.expected_final_url.value_or(GURL()).spec().c_str(),
        test_case.should_notify_withheld));

    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    ActionInfo redirect_action_info =
        composite_matcher->GetBeforeRequestAction(params, test_case.access);

    EXPECT_EQ(test_case.should_notify_withheld,
              redirect_action_info.notify_request_withheld);
  }
}

// Tests CompositeMatcher with the HostPermissionsAlwaysRequired::kTrue mode.
TEST_F(CompositeMatcherTest, HostPermissionsAlwaysRequired) {
  int rule_id = kMinValidID;

  TestRule block_rule = CreateGenericRule(rule_id++);
  block_rule.condition->url_filter = "example.com";

  TestRule block_rule_2 = CreateGenericRule(rule_id++);
  block_rule_2.condition->url_filter = "foo.com";
  block_rule.priority = 3;

  TestRule allow_rule = CreateGenericRule(rule_id++);
  allow_rule.action->type = "allow";
  allow_rule.condition->url_filter = "foo.com";
  allow_rule.priority = 5;

  TestRule upgrade_rule = CreateGenericRule(rule_id++);
  upgrade_rule.action->type = "upgradeScheme";
  upgrade_rule.condition->url_filter = "upgrade.com";

  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_TRUE(CreateVerifiedMatcher(
      {block_rule, block_rule_2, allow_rule, upgrade_rule},
      CreateTemporarySource(), &matcher));
  CompositeMatcher::MatcherList matchers;
  matchers.push_back(std::move(matcher));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kTrue);

  struct TestCases {
    const char* url;
    const PageAccess access;
    const bool expected_notify_withheld;
    absl::optional<int> expected_matched_rule_id;
  } cases[] = {
      {"https://example.com", PageAccess::kAllowed, false, block_rule.id},
      {"https://example.com", PageAccess::kWithheld, true, absl::nullopt},
      {"https://foo.com", PageAccess::kAllowed, false, allow_rule.id},
      // We don't expect to be notified about this (even though there's a rule
      // that would have matched) because the rule would just allow the request.
      {"https://foo.com", PageAccess::kWithheld, false, absl::nullopt},
      {"http://upgrade.com", PageAccess::kAllowed, false, upgrade_rule.id},
      {"http://upgrade.com", PageAccess::kWithheld, true, absl::nullopt},
      {"http://nomatch.com", PageAccess::kAllowed, false, absl::nullopt},
      {"http://nomatch.com", PageAccess::kWithheld, false, absl::nullopt},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing case %zu", i));

    GURL url(cases[i].url);
    RequestParams params;
    params.url = &url;

    ActionInfo info =
        composite_matcher->GetBeforeRequestAction(params, cases[i].access);
    EXPECT_EQ(cases[i].expected_notify_withheld, info.notify_request_withheld);

    absl::optional<int> rule_matched_id;
    if (info.action)
      rule_matched_id = info.action->rule_id;

    EXPECT_EQ(cases[i].expected_matched_rule_id, rule_matched_id);
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

  std::unique_ptr<RulesetMatcher> matcher_1;
  ASSERT_TRUE(CreateVerifiedMatcher({abc_redirect, def_upgrade, ghi_redirect},
                                    CreateTemporarySource(), &matcher_1));

  // Create a composite matcher.
  std::vector<std::unique_ptr<RulesetMatcher>> matchers;
  matchers.push_back(std::move(matcher_1));
  auto composite_matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

  struct {
    GURL request_url;
    absl::optional<GURL> expected_final_url;
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

      // The request will not be redirected as it matches the upgrade rule but
      // is already https.
      {GURL("https://abcdef.com"), absl::nullopt},

      {GURL("http://xyz.com"), absl::nullopt},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "Test redirect from %s to %s", test_case.request_url.spec().c_str(),
        test_case.expected_final_url.value_or(GURL()).spec().c_str()));

    RequestParams params;
    params.url = &test_case.request_url;
    params.element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
    params.is_third_party = false;

    ActionInfo redirect_action_info =
        composite_matcher->GetBeforeRequestAction(params, PageAccess::kAllowed);

    if (test_case.expected_final_url) {
      ASSERT_TRUE(redirect_action_info.action);
      EXPECT_TRUE(redirect_action_info.action->IsRedirectOrUpgrade());
      EXPECT_EQ(test_case.expected_final_url,
                redirect_action_info.action->redirect_url);
    } else {
      EXPECT_FALSE(redirect_action_info.action.has_value());
    }

    EXPECT_FALSE(redirect_action_info.notify_request_withheld);
  }
}

// Ensure rule placement doesn't have side effects on matching priority.
TEST_F(CompositeMatcherTest, RulePlacement) {
  TestRule block_rule = CreateGenericRule(kMinValidID);
  block_rule.priority = 2;
  block_rule.condition->url_filter = "example.com";

  TestRule redirect_rule = CreateGenericRule(kMinValidID + 1);
  redirect_rule.priority = 3;
  redirect_rule.condition->url_filter = "example.com";
  redirect_rule.action->type = "redirect";
  redirect_rule.action->redirect.emplace();
  redirect_rule.action->redirect->url = "http://newurl.com";

  auto test_matchers = [](CompositeMatcher::MatcherList matchers) {
    auto composite_matcher = std::make_unique<CompositeMatcher>(
        std::move(matchers), HostPermissionsAlwaysRequired::kFalse);

    GURL url("http://example.com");
    RequestParams params;
    params.url = &url;

    ActionInfo info =
        composite_matcher->GetBeforeRequestAction(params, PageAccess::kAllowed);
    ASSERT_TRUE(info.action);
    EXPECT_EQ(kMinValidID + 1u, info.action->rule_id);
    EXPECT_FALSE(info.notify_request_withheld);

    // The highest priority matching rule (`redirect_rule`) needs host
    // permissions to match.
    info = composite_matcher->GetBeforeRequestAction(params,
                                                     PageAccess::kWithheld);
    EXPECT_FALSE(info.action);
    EXPECT_TRUE(info.notify_request_withheld);

    info =
        composite_matcher->GetBeforeRequestAction(params, PageAccess::kDenied);
    EXPECT_FALSE(info.action);
    EXPECT_FALSE(info.notify_request_withheld);
  };

  // Case 1: Both rules are part of the same ruleset.
  {
    SCOPED_TRACE("Same ruleset");
    std::unique_ptr<RulesetMatcher> matcher;
    ASSERT_TRUE(CreateVerifiedMatcher({block_rule, redirect_rule},
                                      CreateTemporarySource(), &matcher));
    std::vector<std::unique_ptr<RulesetMatcher>> matchers;
    matchers.push_back(std::move(matcher));
    test_matchers(std::move(matchers));
  }

  // Case 2: Both rules are part of different rulesets.
  {
    SCOPED_TRACE("Different ruleset");
    std::unique_ptr<RulesetMatcher> block_matcher;
    ASSERT_TRUE(CreateVerifiedMatcher(
        {block_rule}, CreateTemporarySource(RulesetID(1)), &block_matcher));
    std::unique_ptr<RulesetMatcher> redirect_matcher;
    ASSERT_TRUE(CreateVerifiedMatcher({redirect_rule},
                                      CreateTemporarySource(RulesetID(2)),
                                      &redirect_matcher));

    std::vector<std::unique_ptr<RulesetMatcher>> matchers;
    matchers.push_back(std::move(block_matcher));
    matchers.push_back(std::move(redirect_matcher));
    test_matchers(std::move(matchers));
  }
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions

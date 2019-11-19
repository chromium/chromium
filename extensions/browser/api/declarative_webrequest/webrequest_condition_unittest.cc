// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_webrequest/webrequest_condition.h"

#include <memory>
#include <set>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionSet;

namespace extensions {

TEST(WebRequestConditionTest, CreateCondition) {
  URLMatcher matcher;

  std::string error;
  std::unique_ptr<WebRequestCondition> result;

  // Test wrong condition name passed.
  error.clear();
  result = WebRequestCondition::Create(
      NULL, matcher.condition_factory(),
      *base::test::ParseJsonDeprecated(
          "{ \"invalid\": \"foobar\", \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          "}"),
      &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong datatype in host_suffix.
  error.clear();
  result = WebRequestCondition::Create(
      NULL, matcher.condition_factory(),
      *base::test::ParseJsonDeprecated(
          "{ \n"
          "  \"url\": [], \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          "}"),
      &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success (can we support multiple criteria?)
  error.clear();
  result = WebRequestCondition::Create(
      NULL, matcher.condition_factory(),
      *base::test::ParseJsonDeprecated(
          "{ \n"
          "  \"resourceType\": [\"main_frame\"], \n"
          "  \"url\": { \"hostSuffix\": \"example.com\" }, \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  const GURL http_url("http://www.example.com");
  WebRequestInfoInitParams match_params;
  match_params.url = http_url;
  match_params.type = content::ResourceType::kMainFrame;
  match_params.web_request_type = WebRequestResourceType::MAIN_FRAME;
  WebRequestInfo match_request_info(std::move(match_params));
  WebRequestData data(&match_request_info, ON_BEFORE_REQUEST);
  WebRequestDataWithMatchIds request_data(&data);
  request_data.url_match_ids = matcher.MatchURL(http_url);
  EXPECT_EQ(1u, request_data.url_match_ids.size());
  EXPECT_TRUE(result->IsFulfilled(request_data));

  const GURL https_url("https://www.example.com");
  WebRequestInfoInitParams wrong_resource_type_params;
  wrong_resource_type_params.url = https_url;
  wrong_resource_type_params.type = content::ResourceType::kSubFrame;
  wrong_resource_type_params.web_request_type =
      WebRequestResourceType::SUB_FRAME;
  WebRequestInfo wrong_resource_type_request_info(
      std::move(wrong_resource_type_params));
  data.request = &wrong_resource_type_request_info;
  request_data.url_match_ids = matcher.MatchURL(http_url);
  // Make sure IsFulfilled does not fail because of URL matching.
  EXPECT_EQ(1u, request_data.url_match_ids.size());
  EXPECT_FALSE(result->IsFulfilled(request_data));
}

TEST(WebRequestConditionTest, CreateConditionFirstPartyForCookies) {
  URLMatcher matcher;

  std::string error;
  std::unique_ptr<WebRequestCondition> result;

  result = WebRequestCondition::Create(
      NULL, matcher.condition_factory(),
      *base::test::ParseJsonDeprecated(
          "{ \n"
          "  \"firstPartyForCookiesUrl\": { \"hostPrefix\": \"fpfc\"}, \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  const GURL http_url("http://www.example.com");
  const GURL first_party_url("http://fpfc.example.com");
  WebRequestInfoInitParams match_params;
  match_params.url = http_url;
  match_params.type = content::ResourceType::kMainFrame;
  match_params.web_request_type = WebRequestResourceType::MAIN_FRAME;
  WebRequestInfo match_request_info(std::move(match_params));
  WebRequestData data(&match_request_info, ON_BEFORE_REQUEST);
  WebRequestDataWithMatchIds request_data(&data);
  request_data.url_match_ids = matcher.MatchURL(http_url);
  EXPECT_EQ(0u, request_data.url_match_ids.size());
  request_data.first_party_url_match_ids = matcher.MatchURL(first_party_url);
  EXPECT_EQ(1u, request_data.first_party_url_match_ids.size());
  EXPECT_TRUE(result->IsFulfilled(request_data));
}

// Conditions without UrlFilter attributes need to be independent of URL
// matching results. We test here that:
//   1. A non-empty condition without UrlFilter attributes is fulfilled iff its
//      attributes are fulfilled.
//   2. An empty condition (in particular, without UrlFilter attributes) is
//      always fulfilled.
TEST(WebRequestConditionTest, NoUrlAttributes) {
  URLMatcher matcher;
  std::string error;

  // The empty condition.
  error.clear();
  std::unique_ptr<WebRequestCondition> condition_empty =
      WebRequestCondition::Create(
          NULL, matcher.condition_factory(),
          *base::test::ParseJsonDeprecated(
              "{ \n"
              "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
              "}"),
          &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition_empty.get());

  // A condition without a UrlFilter attribute, which is always true.
  error.clear();
  std::unique_ptr<WebRequestCondition> condition_no_url_true =
      WebRequestCondition::Create(
          NULL, matcher.condition_factory(),
          *base::test::ParseJsonDeprecated(
              "{ \n"
              "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", "
              "\n"
              // There is no "1st party for cookies" URL in the requests below,
              // therefore all requests are considered first party for cookies.
              "  \"thirdPartyForCookies\": false, \n"
              "}"),
          &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition_no_url_true.get());

  // A condition without a UrlFilter attribute, which is always false.
  error.clear();
  std::unique_ptr<WebRequestCondition> condition_no_url_false =
      WebRequestCondition::Create(
          NULL, matcher.condition_factory(),
          *base::test::ParseJsonDeprecated(
              "{ \n"
              "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", "
              "\n"
              "  \"thirdPartyForCookies\": true, \n"
              "}"),
          &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition_no_url_false.get());

  WebRequestInfoInitParams params;
  params.url = GURL("https://www.example.com");
  params.site_for_cookies = GURL("https://www.example.com");
  WebRequestInfo https_request_info(std::move(params));

  // 1. A non-empty condition without UrlFilter attributes is fulfilled iff its
  //    attributes are fulfilled.
  WebRequestData data(&https_request_info, ON_BEFORE_REQUEST);
  EXPECT_FALSE(
      condition_no_url_false->IsFulfilled(WebRequestDataWithMatchIds(&data)));

  data = WebRequestData(&https_request_info, ON_BEFORE_REQUEST);
  EXPECT_TRUE(
      condition_no_url_true->IsFulfilled(WebRequestDataWithMatchIds(&data)));

  // 2. An empty condition (in particular, without UrlFilter attributes) is
  //    always fulfilled.
  data = WebRequestData(&https_request_info, ON_BEFORE_REQUEST);
  EXPECT_TRUE(condition_empty->IsFulfilled(WebRequestDataWithMatchIds(&data)));
}

TEST(WebRequestConditionTest, CreateConditionSet) {
  URLMatcher matcher;

  WebRequestConditionSet::Values conditions;
  conditions.push_back(base::test::ParseJsonDeprecated(
      "{ \n"
      "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "  \"url\": { \n"
      "    \"hostSuffix\": \"example.com\", \n"
      "    \"schemes\": [\"http\"], \n"
      "  }, \n"
      "}"));
  conditions.push_back(base::test::ParseJsonDeprecated(
      "{ \n"
      "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "  \"url\": { \n"
      "    \"hostSuffix\": \"example.com\", \n"
      "    \"hostPrefix\": \"www\", \n"
      "    \"schemes\": [\"https\"], \n"
      "  }, \n"
      "}"));

  // Test insertion
  std::string error;
  std::unique_ptr<WebRequestConditionSet> result =
      WebRequestConditionSet::Create(NULL, matcher.condition_factory(),
                                     conditions, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(2u, result->conditions().size());

  // Tell the URLMatcher about our shiny new patterns.
  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  // Test that the result is correct and matches http://www.example.com and
  // https://www.example.com
  GURL http_url("http://www.example.com");
  WebRequestInfoInitParams http_params;
  http_params.url = http_url;
  WebRequestInfo http_request_info(std::move(http_params));
  WebRequestData data(&http_request_info, ON_BEFORE_REQUEST);
  WebRequestDataWithMatchIds request_data(&data);
  request_data.url_match_ids = matcher.MatchURL(http_url);
  EXPECT_EQ(1u, request_data.url_match_ids.size());
  EXPECT_TRUE(result->IsFulfilled(*(request_data.url_match_ids.begin()),
                                  request_data));

  GURL https_url("https://www.example.com");
  request_data.url_match_ids = matcher.MatchURL(https_url);
  EXPECT_EQ(1u, request_data.url_match_ids.size());
  WebRequestInfoInitParams https_params;
  https_params.url = https_url;
  WebRequestInfo https_request_info(std::move(https_params));
  data.request = &https_request_info;
  EXPECT_TRUE(result->IsFulfilled(*(request_data.url_match_ids.begin()),
                                  request_data));

  // Check that both, hostPrefix and hostSuffix are evaluated.
  GURL https_foo_url("https://foo.example.com");
  request_data.url_match_ids = matcher.MatchURL(https_foo_url);
  EXPECT_EQ(0u, request_data.url_match_ids.size());
  WebRequestInfoInitParams https_foo_params;
  https_foo_params.url = https_foo_url;
  WebRequestInfo https_foo_request_info(std::move(https_foo_params));
  data.request = &https_foo_request_info;
  EXPECT_FALSE(result->IsFulfilled(-1, request_data));
}

TEST(WebRequestConditionTest, TestPortFilter) {
  URLMatcher matcher;

  WebRequestConditionSet::Values conditions;
  conditions.push_back(base::test::ParseJsonDeprecated(
      "{ \n"
      "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "  \"url\": { \n"
      "    \"ports\": [80, [1000, 1010]], \n"  // Allow 80;1000-1010.
      "    \"hostSuffix\": \"example.com\", \n"
      "  }, \n"
      "}"));

  // Test insertion
  std::string error;
  std::unique_ptr<WebRequestConditionSet> result =
      WebRequestConditionSet::Create(NULL, matcher.condition_factory(),
                                     conditions, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(1u, result->conditions().size());

  // Tell the URLMatcher about our shiny new patterns.
  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  std::set<URLMatcherConditionSet::ID> url_match_ids;

  // Test various URLs.
  GURL http_url("http://www.example.com");
  url_match_ids = matcher.MatchURL(http_url);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_80("http://www.example.com:80");
  url_match_ids = matcher.MatchURL(http_url_80);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_1000("http://www.example.com:1000");
  url_match_ids = matcher.MatchURL(http_url_1000);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_2000("http://www.example.com:2000");
  url_match_ids = matcher.MatchURL(http_url_2000);
  ASSERT_EQ(0u, url_match_ids.size());
}

// Create a condition with two attributes: one on the request header and one on
// the response header. The Create() method should fail and complain that it is
// impossible that both conditions are fulfilled at the same time.
TEST(WebRequestConditionTest, ConditionsWithConflictingStages) {
  URLMatcher matcher;

  std::string error;
  std::unique_ptr<WebRequestCondition> result;

  // Test error on incompatible application stages for involved attributes.
  error.clear();
  result = WebRequestCondition::Create(
      NULL, matcher.condition_factory(),
      *base::test::ParseJsonDeprecated(
          "{ \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          // Pass a JS array with one empty object to each of the header
          // filters.
          "  \"requestHeaders\": [{}], \n"
          "  \"responseHeaders\": [{}], \n"
          "}"),
      &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());
}

}  // namespace extensions

// Copyright 2012 The Chromium Authors
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
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
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
  constexpr const char kWrongNameCondition[] = R"({
    "invalid": "foobar",
    "instanceType": "declarativeWebRequest.RequestMatcher",
  })";
  result = WebRequestCondition::Create(
      nullptr, matcher.condition_factory(),
      base::test::ParseJson(kWrongNameCondition), &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong datatype in host_suffix.
  constexpr const char kWrongDataTypeCondition[] = R"({
    "url": [],
    "instanceType": "declarativeWebRequest.RequestMatcher",
  })";
  error.clear();
  result = WebRequestCondition::Create(
      nullptr, matcher.condition_factory(),
      base::test::ParseJson(kWrongDataTypeCondition), &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success (can we support multiple criteria?)
  error.clear();
  constexpr const char kMultipleCriteriaCondition[] = R"({
    "resourceType": ["main_frame"],
    "url": { "hostSuffix": "example.com" },
    "instanceType": "declarativeWebRequest.RequestMatcher",
  })";
  result = WebRequestCondition::Create(
      nullptr, matcher.condition_factory(),
      base::test::ParseJson(kMultipleCriteriaCondition), &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  const GURL http_url("http://www.example.com");
  WebRequestInfoInitParams match_params;
  match_params.url = http_url;
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

TEST(WebRequestConditionTest, IgnoreConditionFirstPartyForCookies) {
  // firstPartyForCookiesUrl is deprecated, but must still be accepted in
  // parsing.
  URLMatcher matcher;

  std::string error;
  std::unique_ptr<WebRequestCondition> result;

  constexpr const char kCondition[] = R"({
    "firstPartyForCookiesUrl": { "hostPrefix": "fpfc"},
    "instanceType": "declarativeWebRequest.RequestMatcher",
  })";
  result =
      WebRequestCondition::Create(nullptr, matcher.condition_factory(),
                                  base::test::ParseJson(kCondition), &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
}

TEST(WebRequestConditionTest, IgnoreConditionThirdPartForCookies) {
  // thirdPartyForCookies is deprecated, but must still be accepted in
  // parsing.
  URLMatcher matcher;

  std::string error;
  std::unique_ptr<WebRequestCondition> result;

  constexpr const char kConditionTrue[] = R"({
    "thirdPartyForCookies": true,
    "instanceType": "declarativeWebRequest.RequestMatcher",
  })";
  result = WebRequestCondition::Create(nullptr, matcher.condition_factory(),
                                       base::test::ParseJson(kConditionTrue),
                                       &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  constexpr const char kConditionFalse[] = R"({
    "thirdPartyForCookies": false,
    "instanceType": "declarativeWebRequest.RequestMatcher",
  })";
  result = WebRequestCondition::Create(nullptr, matcher.condition_factory(),
                                       base::test::ParseJson(kConditionFalse),
                                       &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
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
  constexpr const char kEmptyCondition[] = R"({
    "instanceType": "declarativeWebRequest.RequestMatcher",
  })";
  std::unique_ptr<WebRequestCondition> condition_empty =
      WebRequestCondition::Create(nullptr, matcher.condition_factory(),
                                  base::test::ParseJson(kEmptyCondition),
                                  &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition_empty.get());

  // A condition without a UrlFilter attribute, which is true.
  error.clear();
  constexpr const char kTrueConditionWithoutUrlFilter[] = R"({
    "instanceType": "declarativeWebRequest.RequestMatcher",

    // This header is set on the WebRequestInfo below.
    "requestHeaders": [{ "nameEquals": "foo" }],
  })";
  std::unique_ptr<WebRequestCondition> condition_no_url_true =
      WebRequestCondition::Create(
          nullptr, matcher.condition_factory(),
          base::test::ParseJson(kTrueConditionWithoutUrlFilter), &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition_no_url_true.get());

  // A condition without a UrlFilter attribute, which is false.
  error.clear();
  constexpr const char kFalseConditionWithoutUrlFilter[] = R"({
    "instanceType": "declarativeWebRequest.RequestMatcher",

    "excludeRequestHeaders": [{ "nameEquals": "foo" }],
  })";
  std::unique_ptr<WebRequestCondition> condition_no_url_false =
      WebRequestCondition::Create(
          nullptr, matcher.condition_factory(),
          base::test::ParseJson(kFalseConditionWithoutUrlFilter), &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition_no_url_false.get());

  WebRequestInfoInitParams params;
  params.url = GURL("https://www.example.com");
  params.extra_request_headers.SetHeader("foo", "bar");
  WebRequestInfo https_request_info(std::move(params));

  // 1. A non-empty condition without UrlFilter attributes is fulfilled iff its
  //    attributes are fulfilled.
  WebRequestData data(&https_request_info, ON_BEFORE_SEND_HEADERS);
  EXPECT_FALSE(
      condition_no_url_false->IsFulfilled(WebRequestDataWithMatchIds(&data)));

  data = WebRequestData(&https_request_info, ON_BEFORE_SEND_HEADERS);
  EXPECT_TRUE(
      condition_no_url_true->IsFulfilled(WebRequestDataWithMatchIds(&data)));

  // 2. An empty condition (in particular, without UrlFilter attributes) is
  //    always fulfilled.
  data = WebRequestData(&https_request_info, ON_BEFORE_SEND_HEADERS);
  EXPECT_TRUE(condition_empty->IsFulfilled(WebRequestDataWithMatchIds(&data)));
}

TEST(WebRequestConditionTest, CreateConditionSet) {
  URLMatcher matcher;

  base::Value::List conditions;
  conditions.Append(base::test::ParseJson(
      "{ \n"
      "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "  \"url\": { \n"
      "    \"hostSuffix\": \"example.com\", \n"
      "    \"schemes\": [\"http\"], \n"
      "  }, \n"
      "}"));
  conditions.Append(base::test::ParseJson(
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
      WebRequestConditionSet::Create(nullptr, matcher.condition_factory(),
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

  base::Value::List conditions;
  conditions.Append(base::test::ParseJson(
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
      WebRequestConditionSet::Create(nullptr, matcher.condition_factory(),
                                     conditions, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(1u, result->conditions().size());

  // Tell the URLMatcher about our shiny new patterns.
  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  std::set<base::MatcherStringPattern::ID> url_match_ids;

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

  // Test error on incompatible application stages for involved attributes.
  constexpr const char kCondition[] = R"({
    "instanceType": "declarativeWebRequest.RequestMatcher",
    // Pass a JS array with one empty object to each of the header
    // filters.
    "requestHeaders": [{}],
    "responseHeaders": [{}],
  })";
  std::unique_ptr<WebRequestCondition> result =
      WebRequestCondition::Create(nullptr, matcher.condition_factory(),
                                  base::test::ParseJson(kCondition), &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());
}

}  // namespace extensions

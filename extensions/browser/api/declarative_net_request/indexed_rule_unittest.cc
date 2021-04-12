// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/indexed_rule.h"

#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = extensions::api::declarative_net_request;

constexpr const char* kTestExtensionId = "extensionid";

GURL GetBaseURL() {
  return Extension::GetBaseURLFromExtensionId(kTestExtensionId);
}

std::unique_ptr<dnr_api::Redirect> MakeRedirectUrl(const char* redirect_url) {
  auto redirect = std::make_unique<dnr_api::Redirect>();
  redirect->url = std::make_unique<std::string>(redirect_url);
  return redirect;
}

dnr_api::Rule CreateGenericParsedRule() {
  dnr_api::Rule rule;
  rule.priority = std::make_unique<int>(kMinValidPriority);
  rule.id = kMinValidID;
  rule.condition.url_filter = std::make_unique<std::string>("filter");
  rule.action.type = dnr_api::RULE_ACTION_TYPE_BLOCK;
  return rule;
}

using IndexedRuleTest = ::testing::Test;

TEST_F(IndexedRuleTest, IDParsing) {
  struct {
    const int id;
    const ParseResult expected_result;
  } cases[] = {
      {kMinValidID - 1, ParseResult::ERROR_INVALID_RULE_ID},
      {kMinValidID, ParseResult::SUCCESS},
      {kMinValidID + 1, ParseResult::SUCCESS},
  };
  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.id = cases[i].id;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(base::checked_cast<uint32_t>(cases[i].id), indexed_rule.id);
  }
}

TEST_F(IndexedRuleTest, PriorityParsing) {
  struct {
    dnr_api::RuleActionType action_type;
    std::unique_ptr<int> priority;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const uint32_t expected_priority;
  } cases[] = {
      {dnr_api::RULE_ACTION_TYPE_REDIRECT,
       std::make_unique<int>(kMinValidPriority - 1),
       ParseResult::ERROR_INVALID_RULE_PRIORITY, kDefaultPriority},
      {dnr_api::RULE_ACTION_TYPE_REDIRECT,
       std::make_unique<int>(kMinValidPriority), ParseResult::SUCCESS,
       kMinValidPriority},
      {dnr_api::RULE_ACTION_TYPE_REDIRECT, nullptr, ParseResult::SUCCESS,
       kDefaultPriority},
      {dnr_api::RULE_ACTION_TYPE_REDIRECT,
       std::make_unique<int>(kMinValidPriority + 1), ParseResult::SUCCESS,
       kMinValidPriority + 1},
      {dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME,
       std::make_unique<int>(kMinValidPriority - 1),
       ParseResult::ERROR_INVALID_RULE_PRIORITY, kDefaultPriority},
      {dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME,
       std::make_unique<int>(kMinValidPriority), ParseResult::SUCCESS,
       kMinValidPriority},
      {dnr_api::RULE_ACTION_TYPE_BLOCK,
       std::make_unique<int>(kMinValidPriority - 1),
       ParseResult::ERROR_INVALID_RULE_PRIORITY, kDefaultPriority},
      {dnr_api::RULE_ACTION_TYPE_BLOCK,
       std::make_unique<int>(kMinValidPriority), ParseResult::SUCCESS,
       kMinValidPriority},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.priority = std::move(cases[i].priority);
    rule.action.type = cases[i].action_type;

    if (cases[i].action_type == dnr_api::RULE_ACTION_TYPE_REDIRECT) {
      rule.action.redirect = MakeRedirectUrl("http://google.com");
    }

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(ComputeIndexedRulePriority(cases[i].expected_priority,
                                           cases[i].action_type),
                indexed_rule.priority);
  }
}

TEST_F(IndexedRuleTest, OptionsParsing) {
  struct {
    const dnr_api::DomainType domain_type;
    const dnr_api::RuleActionType action_type;
    std::unique_ptr<bool> is_url_filter_case_sensitive;
    const uint8_t expected_options;
  } cases[] = {
      {dnr_api::DOMAIN_TYPE_NONE, dnr_api::RULE_ACTION_TYPE_BLOCK, nullptr,
       flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY |
           flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY},
      {dnr_api::DOMAIN_TYPE_FIRSTPARTY, dnr_api::RULE_ACTION_TYPE_ALLOW,
       std::make_unique<bool>(true),
       flat_rule::OptionFlag_IS_ALLOWLIST |
           flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY},
      {dnr_api::DOMAIN_TYPE_FIRSTPARTY, dnr_api::RULE_ACTION_TYPE_ALLOW,
       std::make_unique<bool>(false),
       flat_rule::OptionFlag_IS_ALLOWLIST |
           flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY |
           flat_rule::OptionFlag_IS_CASE_INSENSITIVE},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.domain_type = cases[i].domain_type;
    rule.action.type = cases[i].action_type;
    rule.condition.is_url_filter_case_sensitive =
        std::move(cases[i].is_url_filter_case_sensitive);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(ParseResult::SUCCESS, result);
    EXPECT_EQ(cases[i].expected_options, indexed_rule.options);
  }
}

TEST_F(IndexedRuleTest, ResourceTypesParsing) {
  using ResourceTypeVec = std::vector<dnr_api::ResourceType>;

  struct {
    std::unique_ptr<ResourceTypeVec> resource_types;
    std::unique_ptr<ResourceTypeVec> excluded_resource_types;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const uint16_t expected_element_types;
  } cases[] = {
      {nullptr, nullptr, ParseResult::SUCCESS,
       flat_rule::ElementType_ANY & ~flat_rule::ElementType_MAIN_FRAME},
      {nullptr,
       std::make_unique<ResourceTypeVec>(
           ResourceTypeVec({dnr_api::RESOURCE_TYPE_SCRIPT})),
       ParseResult::SUCCESS,
       flat_rule::ElementType_ANY & ~flat_rule::ElementType_SCRIPT},
      {std::make_unique<ResourceTypeVec>(ResourceTypeVec(
           {dnr_api::RESOURCE_TYPE_SCRIPT, dnr_api::RESOURCE_TYPE_IMAGE})),
       nullptr, ParseResult::SUCCESS,
       flat_rule::ElementType_SCRIPT | flat_rule::ElementType_IMAGE},
      {std::make_unique<ResourceTypeVec>(ResourceTypeVec(
           {dnr_api::RESOURCE_TYPE_SCRIPT, dnr_api::RESOURCE_TYPE_IMAGE})),
       std::make_unique<ResourceTypeVec>(
           ResourceTypeVec({dnr_api::RESOURCE_TYPE_SCRIPT})),
       ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED,
       flat_rule::ElementType_NONE},
      {nullptr,
       std::make_unique<ResourceTypeVec>(ResourceTypeVec(
           {dnr_api::RESOURCE_TYPE_MAIN_FRAME, dnr_api::RESOURCE_TYPE_SUB_FRAME,
            dnr_api::RESOURCE_TYPE_STYLESHEET, dnr_api::RESOURCE_TYPE_SCRIPT,
            dnr_api::RESOURCE_TYPE_IMAGE, dnr_api::RESOURCE_TYPE_FONT,
            dnr_api::RESOURCE_TYPE_OBJECT,
            dnr_api::RESOURCE_TYPE_XMLHTTPREQUEST, dnr_api::RESOURCE_TYPE_PING,
            dnr_api::RESOURCE_TYPE_CSP_REPORT, dnr_api::RESOURCE_TYPE_MEDIA,
            dnr_api::RESOURCE_TYPE_WEBSOCKET, dnr_api::RESOURCE_TYPE_OTHER})),
       ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES,
       flat_rule::ElementType_NONE},
      {std::make_unique<ResourceTypeVec>(), std::make_unique<ResourceTypeVec>(),
       ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST,
       flat_rule::ElementType_NONE},
      {std::make_unique<ResourceTypeVec>(
           ResourceTypeVec({dnr_api::RESOURCE_TYPE_SCRIPT})),
       std::make_unique<ResourceTypeVec>(ResourceTypeVec()),
       ParseResult::SUCCESS, flat_rule::ElementType_SCRIPT},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.resource_types = std::move(cases[i].resource_types);
    rule.condition.excluded_resource_types =
        std::move(cases[i].excluded_resource_types);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_element_types, indexed_rule.element_types);
  }
}

TEST_F(IndexedRuleTest, UrlFilterParsing) {
  struct {
    std::unique_ptr<std::string> input_url_filter;

    // Only valid if |expected_result| is SUCCESS.
    const flat_rule::UrlPatternType expected_url_pattern_type;
    const flat_rule::AnchorType expected_anchor_left;
    const flat_rule::AnchorType expected_anchor_right;
    const std::string expected_url_pattern;

    const ParseResult expected_result;
  } cases[] = {
      {nullptr, flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
       flat_rule::AnchorType_NONE, "", ParseResult::SUCCESS},
      {std::make_unique<std::string>(""), flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "",
       ParseResult::ERROR_EMPTY_URL_FILTER},
      {std::make_unique<std::string>("|"), flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_BOUNDARY, flat_rule::AnchorType_NONE, "",
       ParseResult::SUCCESS},
      {std::make_unique<std::string>("||"), flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_NONE, "",
       ParseResult::SUCCESS},
      {std::make_unique<std::string>("|||"),
       flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
       flat_rule::AnchorType_BOUNDARY, "", ParseResult::SUCCESS},
      {std::make_unique<std::string>("|*|||"),
       flat_rule::UrlPatternType_WILDCARDED, flat_rule::AnchorType_BOUNDARY,
       flat_rule::AnchorType_BOUNDARY, "*||", ParseResult::SUCCESS},
      {std::make_unique<std::string>("|xyz|"),
       flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_BOUNDARY,
       flat_rule::AnchorType_BOUNDARY, "xyz", ParseResult::SUCCESS},
      {std::make_unique<std::string>("||x^yz"),
       flat_rule::UrlPatternType_WILDCARDED, flat_rule::AnchorType_SUBDOMAIN,
       flat_rule::AnchorType_NONE, "x^yz", ParseResult::SUCCESS},
      {std::make_unique<std::string>("||xyz|"),
       flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
       flat_rule::AnchorType_BOUNDARY, "xyz", ParseResult::SUCCESS},
      {std::make_unique<std::string>("x*y|z"),
       flat_rule::UrlPatternType_WILDCARDED, flat_rule::AnchorType_NONE,
       flat_rule::AnchorType_NONE, "x*y|z", ParseResult::SUCCESS},
      {std::make_unique<std::string>("**^"),
       flat_rule::UrlPatternType_WILDCARDED, flat_rule::AnchorType_NONE,
       flat_rule::AnchorType_NONE, "**^", ParseResult::SUCCESS},
      {std::make_unique<std::string>("||google.com"),
       flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
       flat_rule::AnchorType_NONE, "google.com", ParseResult::SUCCESS},
      // Url pattern with non-ascii characters -ⱴase.com.
      {std::make_unique<std::string>(base::WideToUTF8(L"\x2c74"
                                                      L"ase.com")),
       flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
       flat_rule::AnchorType_NONE, "", ParseResult::ERROR_NON_ASCII_URL_FILTER},
      // Url pattern starting with the domain anchor followed by a wildcard.
      {std::make_unique<std::string>("||*xyz"),
       flat_rule::UrlPatternType_WILDCARDED, flat_rule::AnchorType_SUBDOMAIN,
       flat_rule::AnchorType_NONE, "", ParseResult::ERROR_INVALID_URL_FILTER}};

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.url_filter = std::move(cases[i].input_url_filter);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);
    if (result != ParseResult::SUCCESS)
      continue;

    EXPECT_EQ(cases[i].expected_result, result);
    EXPECT_EQ(cases[i].expected_url_pattern_type,
              indexed_rule.url_pattern_type);
    EXPECT_EQ(cases[i].expected_anchor_left, indexed_rule.anchor_left);
    EXPECT_EQ(cases[i].expected_anchor_right, indexed_rule.anchor_right);
    EXPECT_EQ(cases[i].expected_url_pattern, indexed_rule.url_pattern);
  }
}

// Ensure case-insensitive patterns are lower-cased as required by
// url_pattern_index.
TEST_F(IndexedRuleTest, CaseInsensitiveLowerCased) {
  const std::string kPattern = "/QUERY";
  struct {
    std::unique_ptr<bool> is_url_filter_case_sensitive;
    std::string expected_pattern;
  } test_cases[] = {
      {std::make_unique<bool>(false), "/query"},
      {std::make_unique<bool>(true), "/QUERY"},
      {nullptr, "/QUERY"}  // By default patterns are case sensitive.
  };

  for (auto& test_case : test_cases) {
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.url_filter = std::make_unique<std::string>(kPattern);
    rule.condition.is_url_filter_case_sensitive =
        std::move(test_case.is_url_filter_case_sensitive);
    IndexedRule indexed_rule;
    ASSERT_EQ(ParseResult::SUCCESS,
              IndexedRule::CreateIndexedRule(std::move(rule), GetBaseURL(),
                                             &indexed_rule));
    EXPECT_EQ(test_case.expected_pattern, indexed_rule.url_pattern);
  }
}

TEST_F(IndexedRuleTest, DomainsParsing) {
  using DomainVec = std::vector<std::string>;
  struct {
    std::unique_ptr<DomainVec> domains;
    std::unique_ptr<DomainVec> excluded_domains;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const DomainVec expected_domains;
    const DomainVec expected_excluded_domains;
  } cases[] = {
      {nullptr, nullptr, ParseResult::SUCCESS, {}, {}},
      {std::make_unique<DomainVec>(),
       nullptr,
       ParseResult::ERROR_EMPTY_DOMAINS_LIST,
       {},
       {}},
      {nullptr, std::make_unique<DomainVec>(), ParseResult::SUCCESS, {}, {}},
      {std::make_unique<DomainVec>(DomainVec({"a.com", "b.com", "a.com"})),
       std::make_unique<DomainVec>(
           DomainVec({"g.com", "XY.COM", "zzz.com", "a.com", "google.com"})),
       ParseResult::SUCCESS,
       {"a.com", "a.com", "b.com"},
       {"google.com", "zzz.com", "xy.com", "a.com", "g.com"}},
      // Domain with non-ascii characters.
      {std::make_unique<DomainVec>(
           DomainVec({base::WideToUTF8(L"abc\x2010" /*hyphen*/ L"def.com")})),
       nullptr,
       ParseResult::ERROR_NON_ASCII_DOMAIN,
       {},
       {}},
      // Excluded domain with non-ascii characters.
      {nullptr,
       std::make_unique<DomainVec>(
           DomainVec({base::WideToUTF8(L"36\x00b0"
                                       L"c.com" /*36°c.com*/)})),
       ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN,
       {},
       {}},
      // Internationalized domain in punycode.
      {std::make_unique<DomainVec>(
           DomainVec({"xn--36c-tfa.com" /* punycode for 36°c.com*/})),
       nullptr,
       ParseResult::SUCCESS,
       {"xn--36c-tfa.com"},
       {}},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.domains = std::move(cases[i].domains);
    rule.condition.excluded_domains = std::move(cases[i].excluded_domains);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS) {
      EXPECT_EQ(cases[i].expected_domains, indexed_rule.domains);
      EXPECT_EQ(cases[i].expected_excluded_domains,
                indexed_rule.excluded_domains);
    }
  }
}

TEST_F(IndexedRuleTest, RedirectUrlParsing) {
  struct {
    const char* redirect_url;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const std::string expected_redirect_url;
  } cases[] = {
      {"", ParseResult::ERROR_INVALID_REDIRECT_URL, ""},
      {"http://google.com", ParseResult::SUCCESS, "http://google.com"},
      {"/relative/url?q=1", ParseResult::ERROR_INVALID_REDIRECT_URL, ""},
      {"abc", ParseResult::ERROR_INVALID_REDIRECT_URL, ""}};

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.action.redirect = MakeRedirectUrl(cases[i].redirect_url);
    rule.action.type = dnr_api::RULE_ACTION_TYPE_REDIRECT;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result) << static_cast<int>(result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_redirect_url, indexed_rule.redirect_url);
  }
}

TEST_F(IndexedRuleTest, RedirectParsing) {
  struct {
    std::string redirect_dictionary_json;
    ParseResult expected_result;
    base::Optional<std::string> expected_redirect_url;
  } cases[] = {
      // clang-format off
    {
      "{}",
      ParseResult::ERROR_INVALID_REDIRECT,
      base::nullopt
    },
    {
      R"({"url": "xyz"})",
      ParseResult::ERROR_INVALID_REDIRECT_URL,
      base::nullopt
    },
    {
      R"({"url": "javascript:window.alert(\"hello,world\");"})",
      ParseResult::ERROR_JAVASCRIPT_REDIRECT,
      base::nullopt
    },
    {
      R"({"url": "http://google.com"})",
      ParseResult::SUCCESS,
      std::string("http://google.com")
    },
    {
      R"({"extensionPath": "foo/xyz/"})",
      ParseResult::ERROR_INVALID_EXTENSION_PATH,
      base::nullopt
    },
    {
      R"({"extensionPath": "/foo/xyz?q=1"})",
      ParseResult::SUCCESS,
      GetBaseURL().Resolve("/foo/xyz?q=1").spec()
    },
    {
      R"(
      {
        "transform": {
          "scheme": "",
          "host": "foo.com"
        }
      })", ParseResult::ERROR_INVALID_TRANSFORM_SCHEME, base::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "javascript",
          "host": "foo.com"
        }
      })", ParseResult::ERROR_INVALID_TRANSFORM_SCHEME, base::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "http",
          "port": "-1"
        }
      })", ParseResult::ERROR_INVALID_TRANSFORM_PORT, base::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "http",
          "query": "abc"
        }
      })", ParseResult::ERROR_INVALID_TRANSFORM_QUERY, base::nullopt
    },
    {
      R"({"transform": {"path": "abc"}})",
      ParseResult::SUCCESS,
      base::nullopt
    },
    {
      R"({"transform": {"fragment": "abc"}})",
      ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT,
      base::nullopt
    },
    {
      R"({"transform": {"path": ""}})",
      ParseResult::SUCCESS,
      base::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "http",
          "query": "?abc",
          "queryTransform": {
            "removeParams": ["abc"]
          }
        }
      })", ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED, base::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "https",
          "host": "foo.com",
          "port": "80",
          "path": "/foo",
          "queryTransform": {
            "removeParams": ["x1", "x2"],
            "addOrReplaceParams": [
              {"key": "y1", "value": "foo"}
            ]
          },
          "fragment": "",
          "username": "user"
        }
      })", ParseResult::SUCCESS, base::nullopt
    }
  };
  // clang-format on

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.action.type = dnr_api::RULE_ACTION_TYPE_REDIRECT;

    base::Optional<base::Value> redirect_val =
        base::JSONReader::Read(cases[i].redirect_dictionary_json);
    ASSERT_TRUE(redirect_val);

    std::u16string error;
    rule.action.redirect = dnr_api::Redirect::FromValue(*redirect_val, &error);
    ASSERT_TRUE(rule.action.redirect);
    ASSERT_TRUE(error.empty());

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);
    EXPECT_EQ(cases[i].expected_result, result) << static_cast<int>(result);
    if (result != ParseResult::SUCCESS)
      continue;

    EXPECT_TRUE(indexed_rule.url_transform || indexed_rule.redirect_url);
    EXPECT_FALSE(indexed_rule.url_transform && indexed_rule.redirect_url);
    EXPECT_EQ(cases[i].expected_redirect_url, indexed_rule.redirect_url);
  }
}

TEST_F(IndexedRuleTest, RegexFilterParsing) {
  struct {
    std::string regex_filter;
    ParseResult result;
  } cases[] = {{"", ParseResult::ERROR_EMPTY_REGEX_FILTER},
               // Filter with non-ascii characters.
               {"αcd", ParseResult::ERROR_NON_ASCII_REGEX_FILTER},
               // Invalid regex: Unterminated character class.
               {"x[ab", ParseResult::ERROR_INVALID_REGEX_FILTER},
               // Invalid regex: Incomplete capturing group.
               {"x(", ParseResult::ERROR_INVALID_REGEX_FILTER},
               // Invalid regex: Invalid escape sequence \x.
               {R"(ij\x1)", ParseResult::ERROR_INVALID_REGEX_FILTER},
               {R"(ij\\x1)", ParseResult::SUCCESS},
               {R"(^http://www\.(abc|def)\.xyz\.com/)", ParseResult::SUCCESS}};

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.regex_filter);
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.url_filter.reset();
    rule.condition.regex_filter =
        std::make_unique<std::string>(test_case.regex_filter);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);
    EXPECT_EQ(result, test_case.result);

    if (result == ParseResult::SUCCESS) {
      EXPECT_EQ(indexed_rule.url_pattern, test_case.regex_filter);
      EXPECT_EQ(flat_rule::UrlPatternType_REGEXP,
                indexed_rule.url_pattern_type);
    }
  }
}

TEST_F(IndexedRuleTest, MultipleFiltersSpecified) {
  dnr_api::Rule rule = CreateGenericParsedRule();
  rule.condition.url_filter = std::make_unique<std::string>("google");
  rule.condition.regex_filter = std::make_unique<std::string>("example");

  IndexedRule indexed_rule;
  ParseResult result = IndexedRule::CreateIndexedRule(
      std::move(rule), GetBaseURL(), &indexed_rule);
  EXPECT_EQ(ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED, result);
}

TEST_F(IndexedRuleTest, RegexSubstitutionParsing) {
  struct {
    // |regex_filter| may be null.
    const char* regex_filter;
    std::string regex_substitution;
    ParseResult result;
  } cases[] = {
      {nullptr, "http://google.com",
       ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER},
      // \0 in |regex_substitution| refers to the entire matching text.
      {R"(^http://(.*)\.com/)", R"(https://redirect.com?referrer=\0)",
       ParseResult::SUCCESS},
      {R"(^http://google\.com?q1=(.*)&q2=(.*))",
       R"(https://redirect.com?&q1=\0&q2=\2)", ParseResult::SUCCESS},
      // Referencing invalid capture group.
      {R"(^http://google\.com?q1=(.*)&q2=(.*))",
       R"(https://redirect.com?&q1=\1&q2=\3)",
       ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION},
      // Empty substitution.
      {R"(^http://(.*)\.com/)", "",
       ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION},
      // '\' must always be followed by a '\' or a digit.
      {R"(^http://(.*)\.com/)", R"(https://example.com?q=\a)",
       ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.regex_substitution);

    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.url_filter.reset();
    if (test_case.regex_filter) {
      rule.condition.regex_filter =
          std::make_unique<std::string>(test_case.regex_filter);
    }

    rule.priority = std::make_unique<int>(kMinValidPriority);
    rule.action.type = dnr_api::RULE_ACTION_TYPE_REDIRECT;
    rule.action.redirect = std::make_unique<dnr_api::Redirect>();
    rule.action.redirect->regex_substitution =
        std::make_unique<std::string>(test_case.regex_substitution);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);
    EXPECT_EQ(test_case.result, result);

    if (result == ParseResult::SUCCESS) {
      EXPECT_EQ(flat_rule::UrlPatternType_REGEXP,
                indexed_rule.url_pattern_type);
      ASSERT_TRUE(indexed_rule.regex_substitution);
      EXPECT_EQ(test_case.regex_substitution, *indexed_rule.regex_substitution);
    }
  }
}

// Tests the parsing behavior when multiple keys in "Redirect" dictionary are
// specified.
TEST_F(IndexedRuleTest, MultipleRedirectKeys) {
  dnr_api::Rule rule = CreateGenericParsedRule();
  rule.priority = std::make_unique<int>(kMinValidPriority);
  rule.condition.url_filter.reset();
  rule.condition.regex_filter = std::make_unique<std::string>("\\.*");
  rule.action.type = dnr_api::RULE_ACTION_TYPE_REDIRECT;
  rule.action.redirect = std::make_unique<dnr_api::Redirect>();

  dnr_api::Redirect& redirect = *rule.action.redirect;
  redirect.url = std::make_unique<std::string>("http://google.com");
  redirect.regex_substitution =
      std::make_unique<std::string>("http://example.com");
  redirect.transform = std::make_unique<dnr_api::URLTransform>();
  redirect.transform->scheme = std::make_unique<std::string>("https");

  IndexedRule indexed_rule;
  ParseResult result = IndexedRule::CreateIndexedRule(
      std::move(rule), GetBaseURL(), &indexed_rule);
  EXPECT_EQ(ParseResult::SUCCESS, result);

  // The redirect "url" is given preference in this case.
  EXPECT_FALSE(indexed_rule.url_transform);
  EXPECT_FALSE(indexed_rule.regex_substitution);
  EXPECT_EQ("http://google.com", indexed_rule.redirect_url);
}

TEST_F(IndexedRuleTest, InvalidAllowAllRequestsResourceType) {
  using ResourceTypeVec = std::vector<dnr_api::ResourceType>;

  struct {
    ResourceTypeVec resource_types;
    ResourceTypeVec excluded_resource_types;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const uint16_t expected_element_types;
  } cases[] = {
      {{}, {}, ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE, 0},
      {{dnr_api::RESOURCE_TYPE_SUB_FRAME},
       {dnr_api::RESOURCE_TYPE_SCRIPT},
       ParseResult::SUCCESS,
       flat_rule::ElementType_SUBDOCUMENT},
      {{dnr_api::RESOURCE_TYPE_SCRIPT, dnr_api::RESOURCE_TYPE_MAIN_FRAME},
       {},
       ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE,
       0},
      {{dnr_api::RESOURCE_TYPE_MAIN_FRAME, dnr_api::RESOURCE_TYPE_SUB_FRAME},
       {},
       ParseResult::SUCCESS,
       flat_rule::ElementType_MAIN_FRAME | flat_rule::ElementType_SUBDOCUMENT},
      {{dnr_api::RESOURCE_TYPE_MAIN_FRAME},
       {},
       ParseResult::SUCCESS,
       flat_rule::ElementType_MAIN_FRAME},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();

    if (cases[i].resource_types.empty())
      rule.condition.resource_types = nullptr;
    else
      rule.condition.resource_types =
          std::make_unique<ResourceTypeVec>(cases[i].resource_types);

    rule.condition.excluded_resource_types =
        std::make_unique<ResourceTypeVec>(cases[i].excluded_resource_types);
    rule.action.type = dnr_api::RULE_ACTION_TYPE_ALLOWALLREQUESTS;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_element_types, indexed_rule.element_types);
  }
}

TEST_F(IndexedRuleTest, ModifyHeadersParsing) {
  struct RawHeaderInfo {
    dnr_api::HeaderOperation operation;
    std::string header;
    base::Optional<std::string> value;
  };

  using RawHeaderInfoList = std::vector<RawHeaderInfo>;
  using ModifyHeaderInfoList = std::vector<dnr_api::ModifyHeaderInfo>;

  // A copy-able version of dnr_api::ModifyHeaderInfo is used for ease of
  // specifying test cases because elements are copied when initializing a
  // vector from an array.
  struct {
    base::Optional<RawHeaderInfoList> request_headers;
    base::Optional<RawHeaderInfoList> response_headers;
    ParseResult expected_result;
  } cases[] = {
      // Raise an error if no headers are specified.
      {base::nullopt, base::nullopt, ParseResult::ERROR_NO_HEADERS_SPECIFIED},

      // Raise an error if the request or response headers list is specified,
      // but empty.
      {RawHeaderInfoList(),
       RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_REMOVE, "set-cookie", base::nullopt}}),
       ParseResult::ERROR_EMPTY_REQUEST_HEADERS_LIST},

      {base::nullopt, RawHeaderInfoList(),
       ParseResult::ERROR_EMPTY_RESPONSE_HEADERS_LIST},

      // Raise an error if a header list contains an empty or invalid header
      // name.
      {base::nullopt,
       RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_REMOVE, "", base::nullopt}}),
       ParseResult::ERROR_INVALID_HEADER_NAME},

      {base::nullopt,
       RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_REMOVE, "<<invalid>>", base::nullopt}}),
       ParseResult::ERROR_INVALID_HEADER_NAME},

      // Raise an error if a header list contains an invalid header value.
      {base::nullopt,
       RawHeaderInfoList({{dnr_api::HEADER_OPERATION_APPEND, "set-cookie",
                           "invalid\nvalue"}}),
       ParseResult::ERROR_INVALID_HEADER_VALUE},

      // Raise an error if a header value is specified for a remove rule.
      {RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_REMOVE, "cookie", "remove"}}),
       base::nullopt, ParseResult::ERROR_HEADER_VALUE_PRESENT},

      // Raise an error if no header value is specified for an append or set
      // rule.
      {RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_SET, "cookie", base::nullopt}}),
       base::nullopt, ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED},

      {base::nullopt,
       RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_APPEND, "set-cookie", base::nullopt}}),
       ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED},

      // Raise an error if a rule specifies a request header to be appended.
      {RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_APPEND, "cookie", "cookie-value"}}),
       base::nullopt, ParseResult::ERROR_APPEND_REQUEST_HEADER_UNSUPPORTED},

      {RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_REMOVE, "cookie", base::nullopt},
            {dnr_api::HEADER_OPERATION_SET, "referer", ""}}),
       base::nullopt, ParseResult::SUCCESS},

      {RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_REMOVE, "referer", base::nullopt}}),
       RawHeaderInfoList(
           {{dnr_api::HEADER_OPERATION_APPEND, "set-cookie", "abcd"}}),
       ParseResult::SUCCESS},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.action.type = dnr_api::RULE_ACTION_TYPE_MODIFYHEADERS;

    ModifyHeaderInfoList expected_request_headers;
    if (cases[i].request_headers) {
      rule.action.request_headers = std::make_unique<ModifyHeaderInfoList>();
      for (auto header : *cases[i].request_headers) {
        rule.action.request_headers->push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));

        expected_request_headers.push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));
      }
    }

    ModifyHeaderInfoList expected_response_headers;
    if (cases[i].response_headers) {
      rule.action.response_headers = std::make_unique<ModifyHeaderInfoList>();
      for (auto header : *cases[i].response_headers) {
        rule.action.response_headers->push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));

        expected_response_headers.push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));
      }
    }

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);
    EXPECT_EQ(cases[i].expected_result, result);
    if (result != ParseResult::SUCCESS)
      continue;

    EXPECT_EQ(dnr_api::RULE_ACTION_TYPE_MODIFYHEADERS,
              indexed_rule.action_type);

    EXPECT_TRUE(std::equal(
        expected_request_headers.begin(), expected_request_headers.end(),
        indexed_rule.request_headers.begin(), EqualsForTesting));
    EXPECT_TRUE(std::equal(
        expected_response_headers.begin(), expected_response_headers.end(),
        indexed_rule.response_headers.begin(), EqualsForTesting));
  }
}

TEST_F(IndexedRuleTest, RequestMethodsParsing) {
  using RequestMethodVec = std::vector<dnr_api::RequestMethod>;

  struct {
    std::unique_ptr<RequestMethodVec> request_methods;
    std::unique_ptr<RequestMethodVec> excluded_request_methods;
    const ParseResult expected_result;
    // Only valid if `expected_result` is SUCCESS.
    const uint16_t expected_request_methods_mask;
  } cases[] = {
      {nullptr, nullptr, ParseResult::SUCCESS, flat_rule::RequestMethod_ANY},
      {nullptr,
       std::make_unique<RequestMethodVec>(
           RequestMethodVec({dnr_api::REQUEST_METHOD_PUT})),
       ParseResult::SUCCESS,
       flat_rule::RequestMethod_ANY & ~flat_rule::RequestMethod_PUT},
      {std::make_unique<RequestMethodVec>(RequestMethodVec(
           {dnr_api::REQUEST_METHOD_DELETE, dnr_api::REQUEST_METHOD_GET})),
       nullptr, ParseResult::SUCCESS,
       flat_rule::RequestMethod_DELETE | flat_rule::RequestMethod_GET},
      {std::make_unique<RequestMethodVec>(RequestMethodVec(
           {dnr_api::REQUEST_METHOD_HEAD, dnr_api::REQUEST_METHOD_OPTIONS,
            dnr_api::REQUEST_METHOD_PATCH})),
       nullptr, ParseResult::SUCCESS,
       flat_rule::RequestMethod_HEAD | flat_rule::RequestMethod_OPTIONS |
           flat_rule::RequestMethod_PATCH},
      {std::make_unique<RequestMethodVec>(
           RequestMethodVec({dnr_api::REQUEST_METHOD_POST})),
       std::make_unique<RequestMethodVec>(
           RequestMethodVec({dnr_api::REQUEST_METHOD_POST})),
       ParseResult::ERROR_REQUEST_METHOD_DUPLICATED,
       flat_rule::RequestMethod_NONE},
      {std::make_unique<RequestMethodVec>(), nullptr,
       ParseResult::ERROR_EMPTY_REQUEST_METHODS_LIST,
       flat_rule::RequestMethod_NONE}};

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.request_methods = std::move(cases[i].request_methods);
    rule.condition.excluded_request_methods =
        std::move(cases[i].excluded_request_methods);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_request_methods_mask,
                indexed_rule.request_methods);
  }
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions

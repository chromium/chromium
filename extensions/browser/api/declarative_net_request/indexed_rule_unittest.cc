// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/indexed_rule.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
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

dnr_api::Redirect MakeRedirectUrl(const char* redirect_url) {
  dnr_api::Redirect redirect;
  redirect.url = redirect_url;
  return redirect;
}

dnr_api::Rule CreateGenericParsedRule() {
  dnr_api::Rule rule;
  rule.priority = kMinValidPriority;
  rule.id = kMinValidID;
  rule.condition.url_filter = "filter";
  rule.action.type = dnr_api::RuleActionType::kBlock;
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
  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.id = cases[i].id;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(base::checked_cast<uint32_t>(cases[i].id), indexed_rule.id);
  }
}

TEST_F(IndexedRuleTest, PriorityParsing) {
  struct {
    dnr_api::RuleActionType action_type;
    absl::optional<int> priority;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const uint32_t expected_priority;
  } cases[] = {
      {dnr_api::RuleActionType::kRedirect, kMinValidPriority - 1,
       ParseResult::ERROR_INVALID_RULE_PRIORITY, kDefaultPriority},
      {dnr_api::RuleActionType::kRedirect, kMinValidPriority,
       ParseResult::SUCCESS, kMinValidPriority},
      {dnr_api::RuleActionType::kRedirect, absl::nullopt, ParseResult::SUCCESS,
       kDefaultPriority},
      {dnr_api::RuleActionType::kRedirect, kMinValidPriority + 1,
       ParseResult::SUCCESS, kMinValidPriority + 1},
      {dnr_api::RuleActionType::kUpgradeScheme, kMinValidPriority - 1,
       ParseResult::ERROR_INVALID_RULE_PRIORITY, kDefaultPriority},
      {dnr_api::RuleActionType::kUpgradeScheme, kMinValidPriority,
       ParseResult::SUCCESS, kMinValidPriority},
      {dnr_api::RuleActionType::kBlock, kMinValidPriority - 1,
       ParseResult::ERROR_INVALID_RULE_PRIORITY, kDefaultPriority},
      {dnr_api::RuleActionType::kBlock, kMinValidPriority, ParseResult::SUCCESS,
       kMinValidPriority},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.priority = std::move(cases[i].priority);
    rule.action.type = cases[i].action_type;

    if (cases[i].action_type == dnr_api::RuleActionType::kRedirect) {
      rule.action.redirect = MakeRedirectUrl("http://google.com");
    }

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);

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
    absl::optional<bool> is_url_filter_case_sensitive;
    const uint8_t expected_options;
  } cases[] = {
      {dnr_api::DomainType::kNone, dnr_api::RuleActionType::kBlock,
       absl::nullopt,
       flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY |
           flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY},
      {dnr_api::DomainType::kFirstParty, dnr_api::RuleActionType::kAllow, true,
       flat_rule::OptionFlag_IS_ALLOWLIST |
           flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY},
      {dnr_api::DomainType::kFirstParty, dnr_api::RuleActionType::kAllow, false,
       flat_rule::OptionFlag_IS_ALLOWLIST |
           flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY |
           flat_rule::OptionFlag_IS_CASE_INSENSITIVE},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.domain_type = cases[i].domain_type;
    rule.action.type = cases[i].action_type;
    rule.condition.is_url_filter_case_sensitive =
        std::move(cases[i].is_url_filter_case_sensitive);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);

    EXPECT_EQ(ParseResult::SUCCESS, result);
    EXPECT_EQ(cases[i].expected_options, indexed_rule.options);
  }
}

TEST_F(IndexedRuleTest, ResourceTypesParsing) {
  using ResourceTypeVec = std::vector<dnr_api::ResourceType>;

  struct {
    absl::optional<ResourceTypeVec> resource_types;
    absl::optional<ResourceTypeVec> excluded_resource_types;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const uint16_t expected_element_types;
  } cases[] = {
      {absl::nullopt, absl::nullopt, ParseResult::SUCCESS,
       flat_rule::ElementType_ANY & ~flat_rule::ElementType_MAIN_FRAME},
      {absl::nullopt, ResourceTypeVec({dnr_api::ResourceType::kScript}),
       ParseResult::SUCCESS,
       flat_rule::ElementType_ANY & ~flat_rule::ElementType_SCRIPT},
      {ResourceTypeVec(
           {dnr_api::ResourceType::kScript, dnr_api::ResourceType::kImage}),
       absl::nullopt, ParseResult::SUCCESS,
       flat_rule::ElementType_SCRIPT | flat_rule::ElementType_IMAGE},
      {ResourceTypeVec(
           {dnr_api::ResourceType::kScript, dnr_api::ResourceType::kImage}),
       ResourceTypeVec({dnr_api::ResourceType::kScript}),
       ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED,
       flat_rule::ElementType_NONE},
      {absl::nullopt,
       ResourceTypeVec(
           {dnr_api::ResourceType::kMainFrame, dnr_api::ResourceType::kSubFrame,
            dnr_api::ResourceType::kStylesheet, dnr_api::ResourceType::kScript,
            dnr_api::ResourceType::kImage, dnr_api::ResourceType::kFont,
            dnr_api::ResourceType::kObject,
            dnr_api::ResourceType::kXmlhttprequest,
            dnr_api::ResourceType::kPing, dnr_api::ResourceType::kCspReport,
            dnr_api::ResourceType::kMedia, dnr_api::ResourceType::kWebsocket,
            dnr_api::ResourceType::kWebtransport,
            dnr_api::ResourceType::kWebbundle, dnr_api::ResourceType::kOther}),
       ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES,
       flat_rule::ElementType_NONE},
      {{{}},
       {{}},
       ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST,
       flat_rule::ElementType_NONE},
      {ResourceTypeVec({dnr_api::ResourceType::kScript}), ResourceTypeVec(),
       ParseResult::SUCCESS, flat_rule::ElementType_SCRIPT},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.resource_types = std::move(cases[i].resource_types);
    rule.condition.excluded_resource_types =
        std::move(cases[i].excluded_resource_types);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_element_types, indexed_rule.element_types);
  }
}

TEST_F(IndexedRuleTest, UrlFilterParsing) {
  struct {
    absl::optional<std::string> input_url_filter;

    // Only valid if |expected_result| is SUCCESS.
    const flat_rule::UrlPatternType expected_url_pattern_type;
    const flat_rule::AnchorType expected_anchor_left;
    const flat_rule::AnchorType expected_anchor_right;
    const std::string expected_url_pattern;

    const ParseResult expected_result;
  } cases[] = {
      {absl::nullopt, flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "",
       ParseResult::SUCCESS},
      {"", flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
       flat_rule::AnchorType_NONE, "", ParseResult::ERROR_EMPTY_URL_FILTER},
      {"|", flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_BOUNDARY,
       flat_rule::AnchorType_NONE, "", ParseResult::SUCCESS},
      {"||", flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_NONE, "",
       ParseResult::SUCCESS},
      {"|||", flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_BOUNDARY, "",
       ParseResult::SUCCESS},
      {"|*|||", flat_rule::UrlPatternType_WILDCARDED,
       flat_rule::AnchorType_BOUNDARY, flat_rule::AnchorType_BOUNDARY, "*||",
       ParseResult::SUCCESS},
      {"|xyz|", flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_BOUNDARY, flat_rule::AnchorType_BOUNDARY, "xyz",
       ParseResult::SUCCESS},
      {"||x^yz", flat_rule::UrlPatternType_WILDCARDED,
       flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_NONE, "x^yz",
       ParseResult::SUCCESS},
      {"||xyz|", flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_BOUNDARY, "xyz",
       ParseResult::SUCCESS},
      {"x*y|z", flat_rule::UrlPatternType_WILDCARDED,
       flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "x*y|z",
       ParseResult::SUCCESS},
      {"**^", flat_rule::UrlPatternType_WILDCARDED, flat_rule::AnchorType_NONE,
       flat_rule::AnchorType_NONE, "**^", ParseResult::SUCCESS},
      {"||google.com", flat_rule::UrlPatternType_SUBSTRING,
       flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_NONE,
       "google.com", ParseResult::SUCCESS},
      // Url pattern with non-ascii characters -ⱴase.com.
      {base::WideToUTF8(L"\x2c74"
                        L"ase.com"),
       flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
       flat_rule::AnchorType_NONE, "", ParseResult::ERROR_NON_ASCII_URL_FILTER},
      // Url pattern starting with the domain anchor followed by a wildcard.
      {"||*xyz", flat_rule::UrlPatternType_WILDCARDED,
       flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_NONE, "",
       ParseResult::ERROR_INVALID_URL_FILTER}};

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.url_filter = std::move(cases[i].input_url_filter);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);
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
    absl::optional<bool> is_url_filter_case_sensitive;
    std::string expected_pattern;
  } test_cases[] = {
      {false, "/query"},
      {true, "/QUERY"},
      {absl::nullopt, "/QUERY"}  // By default patterns are case sensitive.
  };

  for (auto& test_case : test_cases) {
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.url_filter = kPattern;
    rule.condition.is_url_filter_case_sensitive =
        std::move(test_case.is_url_filter_case_sensitive);
    IndexedRule indexed_rule;
    ASSERT_EQ(ParseResult::SUCCESS,
              IndexedRule::CreateIndexedRule(std::move(rule), GetBaseURL(),
                                             kMinValidStaticRulesetID,
                                             &indexed_rule));
    EXPECT_EQ(test_case.expected_pattern, indexed_rule.url_pattern);
  }
}

TEST_F(IndexedRuleTest, DomainsParsing) {
  using DomainVec = std::vector<std::string>;
  struct {
    absl::optional<DomainVec> domains;
    absl::optional<DomainVec> excluded_domains;
    const ParseResult expected_result;
    // Only valid if |expected_result| is SUCCESS.
    const DomainVec expected_domains;
    const DomainVec expected_excluded_domains;
  } cases[] = {
      {absl::nullopt, absl::nullopt, ParseResult::SUCCESS, {}, {}},
      {{{}}, absl::nullopt, ParseResult::ERROR_EMPTY_DOMAINS_LIST, {}, {}},
      {absl::nullopt, {{}}, ParseResult::SUCCESS, {}, {}},
      {DomainVec({"a.com", "b.com", "a.com"}),
       DomainVec({"g.com", "XY.COM", "zzz.com", "a.com", "google.com"}),
       ParseResult::SUCCESS,
       {"a.com", "a.com", "b.com"},
       {"google.com", "zzz.com", "xy.com", "a.com", "g.com"}},
      // Domain with non-ascii characters.
      {DomainVec({base::WideToUTF8(L"abc\x2010" /*hyphen*/ L"def.com")}),
       absl::nullopt,
       ParseResult::ERROR_NON_ASCII_DOMAIN,
       {},
       {}},
      // Excluded domain with non-ascii characters.
      {absl::nullopt,
       DomainVec({base::WideToUTF8(L"36\x00b0c.com" /*36°c.com*/)}),
       ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN,
       {},
       {}},
      // Internationalized domain in punycode.
      {DomainVec({"xn--36c-tfa.com" /* punycode for 36°c.com*/}),
       absl::nullopt,
       ParseResult::SUCCESS,
       {"xn--36c-tfa.com"},
       {}},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule domains_rule = CreateGenericParsedRule();
    dnr_api::Rule initiator_domains_rule = CreateGenericParsedRule();
    dnr_api::Rule request_domains_rule = CreateGenericParsedRule();

    if (cases[i].domains) {
      domains_rule.condition.domains = *cases[i].domains;
      initiator_domains_rule.condition.initiator_domains = *cases[i].domains;
      request_domains_rule.condition.request_domains = *cases[i].domains;
    }

    if (cases[i].excluded_domains) {
      domains_rule.condition.excluded_domains = *cases[i].excluded_domains;
      initiator_domains_rule.condition.excluded_initiator_domains =
          *cases[i].excluded_domains;
      request_domains_rule.condition.excluded_request_domains =
          *cases[i].excluded_domains;
    }

    IndexedRule indexed_domains_rule;
    ParseResult domains_result = IndexedRule::CreateIndexedRule(
        std::move(domains_rule), GetBaseURL(), kMinValidStaticRulesetID,
        &indexed_domains_rule);

    IndexedRule indexed_initiator_domains_rule;
    ParseResult initiator_domains_result = IndexedRule::CreateIndexedRule(
        std::move(initiator_domains_rule), GetBaseURL(),
        kMinValidStaticRulesetID, &indexed_initiator_domains_rule);

    IndexedRule indexed_request_domains_rule;
    ParseResult request_domains_result = IndexedRule::CreateIndexedRule(
        std::move(request_domains_rule), GetBaseURL(), kMinValidStaticRulesetID,
        &indexed_request_domains_rule);

    EXPECT_EQ(cases[i].expected_result, domains_result);

    switch (cases[i].expected_result) {
      case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
        EXPECT_EQ(ParseResult::ERROR_EMPTY_INITIATOR_DOMAINS_LIST,
                  initiator_domains_result);
        EXPECT_EQ(ParseResult::ERROR_EMPTY_REQUEST_DOMAINS_LIST,
                  request_domains_result);
        break;
      case ParseResult::ERROR_NON_ASCII_DOMAIN:
        EXPECT_EQ(ParseResult::ERROR_NON_ASCII_INITIATOR_DOMAIN,
                  initiator_domains_result);
        EXPECT_EQ(ParseResult::ERROR_NON_ASCII_REQUEST_DOMAIN,
                  request_domains_result);
        break;
      case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
        EXPECT_EQ(ParseResult::ERROR_NON_ASCII_EXCLUDED_INITIATOR_DOMAIN,
                  initiator_domains_result);
        EXPECT_EQ(ParseResult::ERROR_NON_ASCII_EXCLUDED_REQUEST_DOMAIN,
                  request_domains_result);
        break;
      default:
        EXPECT_EQ(cases[i].expected_result, initiator_domains_result);
        EXPECT_EQ(cases[i].expected_result, request_domains_result);
    }

    if (cases[i].expected_result == ParseResult::SUCCESS) {
      // The `domains` and `excluded_domains` rule conditions are deprecated and
      // mapped to `initiator_domains` and `excluded_initiator_domains`.
      EXPECT_EQ(cases[i].expected_domains,
                indexed_domains_rule.initiator_domains);
      EXPECT_EQ(cases[i].expected_excluded_domains,
                indexed_domains_rule.excluded_initiator_domains);

      EXPECT_EQ(cases[i].expected_domains,
                indexed_initiator_domains_rule.initiator_domains);
      EXPECT_EQ(cases[i].expected_excluded_domains,
                indexed_initiator_domains_rule.excluded_initiator_domains);

      EXPECT_EQ(cases[i].expected_domains,
                indexed_request_domains_rule.request_domains);
      EXPECT_EQ(cases[i].expected_excluded_domains,
                indexed_request_domains_rule.excluded_request_domains);
    }
  }

  // Test that attempting to include both domains + initiatorDomains, or
  // excludedDomains + excludedInitiatorDomains results in an parsing error.
  dnr_api::Rule both_domains_rule = CreateGenericParsedRule();
  dnr_api::Rule both_excluded_domains_rule = CreateGenericParsedRule();
  both_domains_rule.condition.domains = DomainVec({"foo"});
  both_domains_rule.condition.initiator_domains = DomainVec({"bar"});
  both_excluded_domains_rule.condition.excluded_domains = DomainVec({"flib"});
  both_excluded_domains_rule.condition.excluded_initiator_domains =
      DomainVec({"flob"});

  IndexedRule indexed_both_domains_rule;
  IndexedRule indexed_both_excluded_domains_rule;
  EXPECT_EQ(ParseResult::ERROR_DOMAINS_AND_INITIATOR_DOMAINS_BOTH_SPECIFIED,
            IndexedRule::CreateIndexedRule(
                std::move(both_domains_rule), GetBaseURL(),
                kMinValidStaticRulesetID, &indexed_both_domains_rule));
  EXPECT_EQ(
      ParseResult::
          ERROR_EXCLUDED_DOMAINS_AND_EXCLUDED_INITIATOR_DOMAINS_BOTH_SPECIFIED,
      IndexedRule::CreateIndexedRule(std::move(both_excluded_domains_rule),
                                     GetBaseURL(), kMinValidStaticRulesetID,
                                     &indexed_both_excluded_domains_rule));
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

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.action.redirect = MakeRedirectUrl(cases[i].redirect_url);
    rule.action.type = dnr_api::RuleActionType::kRedirect;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result) << static_cast<int>(result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_redirect_url, indexed_rule.redirect_url);
  }
}

TEST_F(IndexedRuleTest, RedirectParsing) {
  struct {
    std::string redirect_dictionary_json;
    ParseResult expected_result;
    absl::optional<std::string> expected_redirect_url;
  } cases[] = {
      // clang-format off
    {
      "{}",
      ParseResult::ERROR_INVALID_REDIRECT,
      absl::nullopt
    },
    {
      R"({"url": "xyz"})",
      ParseResult::ERROR_INVALID_REDIRECT_URL,
      absl::nullopt
    },
    {
      R"({"url": "javascript:window.alert(\"hello,world\");"})",
      ParseResult::ERROR_JAVASCRIPT_REDIRECT,
      absl::nullopt
    },
    {
      R"({"url": "http://google.com"})",
      ParseResult::SUCCESS,
      std::string("http://google.com")
    },
    {
      R"({"extensionPath": "foo/xyz/"})",
      ParseResult::ERROR_INVALID_EXTENSION_PATH,
      absl::nullopt
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
      })", ParseResult::ERROR_INVALID_TRANSFORM_SCHEME, absl::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "javascript",
          "host": "foo.com"
        }
      })", ParseResult::ERROR_INVALID_TRANSFORM_SCHEME, absl::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "http",
          "port": "-1"
        }
      })", ParseResult::ERROR_INVALID_TRANSFORM_PORT, absl::nullopt
    },
    {
      R"(
      {
        "transform": {
          "scheme": "http",
          "query": "abc"
        }
      })", ParseResult::ERROR_INVALID_TRANSFORM_QUERY, absl::nullopt
    },
    {
      R"({"transform": {"path": "abc"}})",
      ParseResult::SUCCESS,
      absl::nullopt
    },
    {
      R"({"transform": {"fragment": "abc"}})",
      ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT,
      absl::nullopt
    },
    {
      R"({"transform": {"path": ""}})",
      ParseResult::SUCCESS,
      absl::nullopt
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
      })", ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED, absl::nullopt
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
      })", ParseResult::SUCCESS, absl::nullopt
    }
  };
  // clang-format on

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.action.type = dnr_api::RuleActionType::kRedirect;

    absl::optional<base::Value> redirect_val =
        base::JSONReader::Read(cases[i].redirect_dictionary_json);
    ASSERT_TRUE(redirect_val);

    std::u16string error;
    rule.action.redirect = std::move(
        *dnr_api::Redirect::FromValueDeprecated(*redirect_val, &error));
    ASSERT_TRUE(rule.action.redirect);
    ASSERT_TRUE(error.empty());

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);
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
    rule.condition.regex_filter = test_case.regex_filter;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);
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
  rule.condition.url_filter = "google";
  rule.condition.regex_filter = "example";

  IndexedRule indexed_rule;
  ParseResult result = IndexedRule::CreateIndexedRule(
      std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);
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
      rule.condition.regex_filter = test_case.regex_filter;
    }

    rule.priority = kMinValidPriority;
    rule.action.type = dnr_api::RuleActionType::kRedirect;
    rule.action.redirect.emplace();
    rule.action.redirect->regex_substitution = test_case.regex_substitution;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);
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
  rule.priority = kMinValidPriority;
  rule.condition.url_filter.reset();
  rule.condition.regex_filter = "\\.*";
  rule.action.type = dnr_api::RuleActionType::kRedirect;
  rule.action.redirect.emplace();

  dnr_api::Redirect& redirect = *rule.action.redirect;
  redirect.url = "http://google.com";
  redirect.regex_substitution = "http://example.com";
  redirect.transform.emplace();
  redirect.transform->scheme = "https";

  IndexedRule indexed_rule;
  ParseResult result = IndexedRule::CreateIndexedRule(
      std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);
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
      {{dnr_api::ResourceType::kSubFrame},
       {dnr_api::ResourceType::kScript},
       ParseResult::SUCCESS,
       flat_rule::ElementType_SUBDOCUMENT},
      {{dnr_api::ResourceType::kScript, dnr_api::ResourceType::kMainFrame},
       {},
       ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE,
       0},
      {{dnr_api::ResourceType::kMainFrame, dnr_api::ResourceType::kSubFrame},
       {},
       ParseResult::SUCCESS,
       flat_rule::ElementType_MAIN_FRAME | flat_rule::ElementType_SUBDOCUMENT},
      {{dnr_api::ResourceType::kMainFrame},
       {},
       ParseResult::SUCCESS,
       flat_rule::ElementType_MAIN_FRAME},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();

    if (cases[i].resource_types.empty()) {
      rule.condition.resource_types = absl::nullopt;
    } else {
      rule.condition.resource_types = cases[i].resource_types;
    }

    rule.condition.excluded_resource_types = cases[i].excluded_resource_types;
    rule.action.type = dnr_api::RuleActionType::kAllowAllRequests;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_element_types, indexed_rule.element_types);
  }
}

TEST_F(IndexedRuleTest, ModifyHeadersParsing) {
  struct RawHeaderInfo {
    dnr_api::HeaderOperation operation;
    std::string header;
    absl::optional<std::string> value;
  };

  using RawHeaderInfoList = std::vector<RawHeaderInfo>;
  using ModifyHeaderInfoList = std::vector<dnr_api::ModifyHeaderInfo>;

  // A copy-able version of dnr_api::ModifyHeaderInfo is used for ease of
  // specifying test cases because elements are copied when initializing a
  // vector from an array.
  struct {
    absl::optional<RawHeaderInfoList> request_headers;
    absl::optional<RawHeaderInfoList> response_headers;
    ParseResult expected_result;
  } cases[] = {
      // Raise an error if no headers are specified.
      {absl::nullopt, absl::nullopt, ParseResult::ERROR_NO_HEADERS_SPECIFIED},

      // Raise an error if the request or response headers list is specified,
      // but empty.
      {RawHeaderInfoList(),
       RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kRemove, "set-cookie", absl::nullopt}}),
       ParseResult::ERROR_EMPTY_REQUEST_HEADERS_LIST},

      {absl::nullopt, RawHeaderInfoList(),
       ParseResult::ERROR_EMPTY_RESPONSE_HEADERS_LIST},

      // Raise an error if a header list contains an empty or invalid header
      // name.
      {absl::nullopt,
       RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kRemove, "", absl::nullopt}}),
       ParseResult::ERROR_INVALID_HEADER_NAME},

      {absl::nullopt,
       RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kRemove, "<<invalid>>", absl::nullopt}}),
       ParseResult::ERROR_INVALID_HEADER_NAME},

      // Raise an error if a header list contains an invalid header value.
      {absl::nullopt,
       RawHeaderInfoList({{dnr_api::HeaderOperation::kAppend, "set-cookie",
                           "invalid\nvalue"}}),
       ParseResult::ERROR_INVALID_HEADER_VALUE},

      // Raise an error if a header value is specified for a remove rule.
      {RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kRemove, "cookie", "remove"}}),
       absl::nullopt, ParseResult::ERROR_HEADER_VALUE_PRESENT},

      // Raise an error if no header value is specified for an append or set
      // rule.
      {RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kSet, "cookie", absl::nullopt}}),
       absl::nullopt, ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED},

      {absl::nullopt,
       RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kAppend, "set-cookie", absl::nullopt}}),
       ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED},

      // Raise an error if a rule specifies an invalid request header to be
      // appended.
      {RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kAppend, "invalid-header", "value"}}),
       absl::nullopt, ParseResult::ERROR_APPEND_INVALID_REQUEST_HEADER},

      {RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kRemove, "cookie", absl::nullopt},
            {dnr_api::HeaderOperation::kSet, "referer", ""},
            {dnr_api::HeaderOperation::kAppend, "accept-language", "en-US"}}),
       absl::nullopt, ParseResult::SUCCESS},

      {RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kRemove, "referer", absl::nullopt}}),
       RawHeaderInfoList(
           {{dnr_api::HeaderOperation::kAppend, "set-cookie", "abcd"}}),
       ParseResult::SUCCESS},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.action.type = dnr_api::RuleActionType::kModifyHeaders;

    ModifyHeaderInfoList expected_request_headers;
    if (cases[i].request_headers) {
      rule.action.request_headers.emplace();
      for (auto header : *cases[i].request_headers) {
        rule.action.request_headers->push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));

        expected_request_headers.push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));
      }
    }

    ModifyHeaderInfoList expected_response_headers;
    if (cases[i].response_headers) {
      rule.action.response_headers.emplace();
      for (auto header : *cases[i].response_headers) {
        rule.action.response_headers->push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));

        expected_response_headers.push_back(CreateModifyHeaderInfo(
            header.operation, header.header, header.value));
      }
    }

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);
    EXPECT_EQ(cases[i].expected_result, result);
    if (result != ParseResult::SUCCESS)
      continue;

    EXPECT_EQ(dnr_api::RuleActionType::kModifyHeaders,
              indexed_rule.action_type);

    EXPECT_TRUE(base::ranges::equal(expected_request_headers,
                                    indexed_rule.request_headers,
                                    EqualsForTesting));
    EXPECT_TRUE(base::ranges::equal(expected_response_headers,
                                    indexed_rule.response_headers,
                                    EqualsForTesting));
  }
}

TEST_F(IndexedRuleTest, RequestMethodsParsing) {
  using RequestMethodVec = std::vector<dnr_api::RequestMethod>;

  struct {
    absl::optional<RequestMethodVec> request_methods;
    absl::optional<RequestMethodVec> excluded_request_methods;
    const ParseResult expected_result;
    // Only valid if `expected_result` is SUCCESS.
    const uint16_t expected_request_methods_mask;
  } cases[] = {
      {absl::nullopt, absl::nullopt, ParseResult::SUCCESS,
       flat_rule::RequestMethod_ANY},
      {absl::nullopt, RequestMethodVec({dnr_api::RequestMethod::kPut}),
       ParseResult::SUCCESS,
       flat_rule::RequestMethod_ANY & ~flat_rule::RequestMethod_PUT},
      {RequestMethodVec(
           {dnr_api::RequestMethod::kDelete, dnr_api::RequestMethod::kGet}),
       absl::nullopt, ParseResult::SUCCESS,
       flat_rule::RequestMethod_DELETE | flat_rule::RequestMethod_GET},
      {RequestMethodVec({dnr_api::RequestMethod::kHead,
                         dnr_api::RequestMethod::kOptions,
                         dnr_api::RequestMethod::kPatch}),
       absl::nullopt, ParseResult::SUCCESS,
       flat_rule::RequestMethod_HEAD | flat_rule::RequestMethod_OPTIONS |
           flat_rule::RequestMethod_PATCH},
      {RequestMethodVec({dnr_api::RequestMethod::kPost}),
       RequestMethodVec({dnr_api::RequestMethod::kPost}),
       ParseResult::ERROR_REQUEST_METHOD_DUPLICATED,
       flat_rule::RequestMethod_NONE},
      {{{}},
       absl::nullopt,
       ParseResult::ERROR_EMPTY_REQUEST_METHODS_LIST,
       flat_rule::RequestMethod_NONE}};

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    rule.condition.request_methods = std::move(cases[i].request_methods);
    rule.condition.excluded_request_methods =
        std::move(cases[i].excluded_request_methods);

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), kMinValidStaticRulesetID, &indexed_rule);

    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS)
      EXPECT_EQ(cases[i].expected_request_methods_mask,
                indexed_rule.request_methods);
  }
}

TEST_F(IndexedRuleTest, TabID) {
  using IntVec = std::vector<int>;
  struct {
    absl::optional<IntVec> tab_ids;
    absl::optional<IntVec> excluded_tab_ids;
    RulesetID ruleset_id;
    ParseResult expected_result;

    // Only relevant if `expected_result` is ParseResult::SUCCESS.
    base::flat_set<int> expected_tab_ids;
    base::flat_set<int> expected_excluded_tab_ids;
  } cases[] = {
      {absl::nullopt, absl::nullopt, kSessionRulesetID, ParseResult::SUCCESS},
      {IntVec(), IntVec({3, 4, 4}), kSessionRulesetID,
       ParseResult::ERROR_EMPTY_TAB_IDS_LIST},
      {IntVec({1, 2}),
       IntVec({3, 4, 3}),
       kSessionRulesetID,
       ParseResult::SUCCESS,
       {1, 2},
       {}},
      {absl::nullopt,
       IntVec({3, 4, 3}),
       kSessionRulesetID,
       ParseResult::SUCCESS,
       {},
       {3, 4}},
      {IntVec({1, 2, 3}), IntVec({5, 2}), kSessionRulesetID,
       ParseResult::ERROR_TAB_ID_DUPLICATED},
      {IntVec({1, 2}), absl::nullopt, kDynamicRulesetID,
       ParseResult::ERROR_TAB_IDS_ON_NON_SESSION_RULE},
      {IntVec({1, 2}), IntVec({3}), kMinValidStaticRulesetID,
       ParseResult::ERROR_TAB_IDS_ON_NON_SESSION_RULE},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    dnr_api::Rule rule = CreateGenericParsedRule();
    if (cases[i].tab_ids) {
      rule.condition.tab_ids = *cases[i].tab_ids;
    }

    if (cases[i].excluded_tab_ids) {
      rule.condition.excluded_tab_ids = *cases[i].excluded_tab_ids;
    }

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule), GetBaseURL(), cases[i].ruleset_id, &indexed_rule);
    EXPECT_EQ(cases[i].expected_result, result);
    if (result == ParseResult::SUCCESS) {
      EXPECT_EQ(cases[i].expected_tab_ids, indexed_rule.tab_ids);
      EXPECT_EQ(cases[i].expected_excluded_tab_ids,
                indexed_rule.excluded_tab_ids);
    }
  }
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions

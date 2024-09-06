// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/format_macros.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::declarative_net_request {
namespace {

namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;
using FlatRulesetIndexerTest = ::testing::Test;

// Helper to convert a flatbuffer string to a std::string.
std::string ToString(const flatbuffers::String* string) {
  DCHECK(string);
  return CreateString<std::string>(*string);
}

// Helper to convert a flatbuffer vector of strings to a std::vector.
std::vector<std::string> ToVector(
    const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>*
        vec) {
  if (!vec) {
    return std::vector<std::string>();
  }
  std::vector<std::string> result;
  result.reserve(vec->size());
  for (auto* str : *vec) {
    result.push_back(ToString(str));
  }
  return result;
}

// Helper to convert a flatbuffer vector of flat::ModifyHeaderInfo to a
// std::vector of dnr_api::ModifyHeaderInfo
std::vector<dnr_api::ModifyHeaderInfo> ToVector(
    const ::flatbuffers::Vector<::flatbuffers::Offset<flat::ModifyHeaderInfo>>*
        vec) {
  if (!vec) {
    return std::vector<dnr_api::ModifyHeaderInfo>();
  }
  std::vector<dnr_api::ModifyHeaderInfo> result;
  result.reserve(vec->size());

  for (auto* flat_header_info : *vec) {
    dnr_api::ModifyHeaderInfo header_info;

    const flat::HeaderOperation flat_operation = flat_header_info->operation();
    const flatbuffers::String* flat_value = flat_header_info->value();
    switch (flat_operation) {
      case flat::HeaderOperation_append:
        header_info.operation = dnr_api::HeaderOperation::kAppend;
        DCHECK(flat_value);
        header_info.value = ToString(flat_value);
        break;
      case flat::HeaderOperation_set:
        header_info.operation = dnr_api::HeaderOperation::kSet;
        DCHECK(flat_value);
        header_info.value = ToString(flat_value);
        break;
      case flat::HeaderOperation_remove:
        header_info.operation = dnr_api::HeaderOperation::kRemove;
        break;
    };

    const flatbuffers::String* flat_header = flat_header_info->header();
    DCHECK(flat_header);
    header_info.header = ToString(flat_header);

    const flatbuffers::String* flat_regex_filter =
        flat_header_info->regex_filter();
    if (flat_regex_filter) {
      header_info.regex_filter = ToString(flat_regex_filter);
    }

    const flatbuffers::String* flat_regex_substitution =
        flat_header_info->regex_substitution();
    if (flat_regex_substitution) {
      header_info.regex_substitution = ToString(flat_regex_substitution);
    }

    DCHECK(flat_header_info->regex_options());
    header_info.regex_options = dnr_api::HeaderRegexOptions();
    header_info.regex_options->match_all =
        flat_header_info->regex_options()->match_all();

    result.push_back(std::move(header_info));
  }

  return result;
}

// Helper to create a generic URLTransform.
dnr_api::URLTransform CreateUrlTransform() {
  const char* transform = R"(
    {
      "scheme" : "http",
      "host" : "foo.com",
      "port" : "80",
      "path" : "",
      "queryTransform" : {
        "removeParams" : ["x1", "x2"],
        "addOrReplaceParams" : [
          {"key" : "y1", "value" : "foo"},
          {"key" : "y2", "value" : "foo2", "replaceOnly": false},
          {"key" : "y3", "value" : "foo3", "replaceOnly": true}
        ]
      },
      "fragment" : "#xx",
      "username" : "user",
      "password" : "pass"
    }
  )";

  auto result =
      dnr_api::URLTransform::FromValue(base::test::ParseJsonDict(transform));
  CHECK(result.has_value());
  return std::move(result).value();
}

// Helper to verify the indexed form of URlTransform created by
// |CreateUrlTransform()|.
bool VerifyUrlTransform(const flat::UrlTransform& flat_transform) {
  auto is_string_equal = [](std::string_view str,
                            const flatbuffers::String* flat_str) {
    return flat_str && ToString(flat_str) == str;
  };

  auto verify_add_or_replace_params = [&flat_transform, &is_string_equal]() {
    if (!flat_transform.add_or_replace_query_params() ||
        flat_transform.add_or_replace_query_params()->size() != 3) {
      return false;
    }

    auto does_query_key_value_match = [&flat_transform, &is_string_equal](
                                          int query_key_index,
                                          std::string_view expected_key,
                                          std::string_view expected_value,
                                          bool expected_replace_only) {
      const flat::QueryKeyValue* query_pair =
          flat_transform.add_or_replace_query_params()->Get(query_key_index);
      CHECK(query_pair);
      return is_string_equal(expected_key, query_pair->key()) &&
             is_string_equal(expected_value, query_pair->value()) &&
             query_pair->replace_only() == expected_replace_only;
    };

    return does_query_key_value_match(0, "y1", "foo", false) &&
           does_query_key_value_match(1, "y2", "foo2", false) &&
           does_query_key_value_match(2, "y3", "foo3", true);
  };

  return is_string_equal("http", flat_transform.scheme()) &&
         is_string_equal("foo.com", flat_transform.host()) &&
         !flat_transform.clear_port() &&
         is_string_equal("80", flat_transform.port()) &&
         flat_transform.clear_path() && !flat_transform.path() &&
         !flat_transform.clear_query() && !flat_transform.query() &&
         flat_transform.remove_query_params() &&
         std::vector<std::string>{"x1", "x2"} ==
             ToVector(flat_transform.remove_query_params()) &&
         verify_add_or_replace_params() && !flat_transform.clear_fragment() &&
         is_string_equal("xx", flat_transform.fragment()) &&
         is_string_equal("user", flat_transform.username()) &&
         is_string_equal("pass", flat_transform.password());
}

// Helper to create an IndexedRule.
IndexedRule CreateIndexedRule(
    uint32_t id,
    uint32_t priority,
    uint8_t options,
    uint16_t element_types,
    uint8_t activation_types,
    flat_rule::UrlPatternType url_pattern_type,
    flat_rule::AnchorType anchor_left,
    flat_rule::AnchorType anchor_right,
    std::string url_pattern,
    std::vector<std::string> domains,
    std::vector<std::string> excluded_domains,
    std::optional<std::string> redirect_url,
    dnr_api::RuleActionType action_type,
    std::optional<dnr_api::URLTransform> url_transform,
    std::optional<std::string> regex_substitution,
    std::vector<dnr_api::ModifyHeaderInfo> request_headers_to_modify,
    std::vector<dnr_api::ModifyHeaderInfo> response_headers_to_modify,
    std::vector<dnr_api::HeaderInfo> response_headers,
    std::vector<dnr_api::HeaderInfo> excluded_response_headers) {
  IndexedRule rule;
  rule.id = id;
  rule.priority = priority;
  rule.options = options;
  rule.element_types = element_types;
  rule.activation_types = activation_types;
  rule.url_pattern_type = url_pattern_type;
  rule.anchor_left = anchor_left;
  rule.anchor_right = anchor_right;
  rule.url_pattern = std::move(url_pattern);
  rule.initiator_domains = std::move(domains);
  rule.excluded_initiator_domains = std::move(excluded_domains);
  rule.redirect_url = std::move(redirect_url);
  rule.action_type = action_type;
  rule.url_transform = std::move(url_transform);
  rule.regex_substitution = std::move(regex_substitution);
  rule.request_headers_to_modify = std::move(request_headers_to_modify);
  rule.response_headers_to_modify = std::move(response_headers_to_modify);
  rule.response_headers = std::move(response_headers);
  rule.excluded_response_headers = std::move(excluded_response_headers);
  return rule;
}

// Compares |indexed_rule| and |rule| for equality. Ignores the redirect url and
// the list of request and response headers since they're not stored as part of
// flat_rule::UrlRule.
bool AreRulesEqual(const IndexedRule* indexed_rule,
                   const flat_rule::UrlRule* rule) {
  CHECK(indexed_rule);
  CHECK(rule);

  return indexed_rule->id == rule->id() &&
         indexed_rule->priority == rule->priority() &&
         indexed_rule->options == rule->options() &&
         indexed_rule->element_types == rule->element_types() &&
         indexed_rule->activation_types == rule->activation_types() &&
         indexed_rule->url_pattern_type == rule->url_pattern_type() &&
         indexed_rule->anchor_left == rule->anchor_left() &&
         indexed_rule->anchor_right == rule->anchor_right() &&
         indexed_rule->url_pattern == ToString(rule->url_pattern()) &&
         indexed_rule->initiator_domains ==
             ToVector(rule->initiator_domains_included()) &&
         indexed_rule->excluded_initiator_domains ==
             ToVector(rule->initiator_domains_excluded()) &&
         indexed_rule->request_domains ==
             ToVector(rule->request_domains_included()) &&
         indexed_rule->excluded_request_domains ==
             ToVector(rule->request_domains_excluded());
}

// Returns all UrlRule(s) in the given |index|.
std::vector<const flat_rule::UrlRule*> GetAllRulesFromIndex(
    const flat_rule::UrlPatternIndex* index) {
  std::vector<const flat_rule::UrlRule*> result;

  // Iterate over all ngrams and add their corresponding rules.
  for (auto* ngram_to_rules : *index->ngram_index()) {
    if (ngram_to_rules == index->ngram_index_empty_slot()) {
      continue;
    }
    for (const auto* rule : *ngram_to_rules->rule_list()) {
      result.push_back(rule);
    }
  }

  // Add all fallback rules.
  for (const auto* rule : *index->fallback_rules()) {
    result.push_back(rule);
  }

  return result;
}

// Verifies that both |rules| and |index| correspond to the same set of rules
// (in different representations).
void VerifyIndexEquality(const std::vector<const IndexedRule*>& rules,
                         const flat_rule::UrlPatternIndex* index) {
  struct RulePair {
    raw_ptr<const IndexedRule> indexed_rule = nullptr;
    raw_ptr<const flat_rule::UrlRule> url_rule = nullptr;
  };

  // Build a map from rule IDs to RulePair(s).
  std::map<uint32_t, RulePair> map;

  for (const auto* rule : rules) {
    EXPECT_EQ(nullptr, map[rule->id].indexed_rule.get());
    map[rule->id].indexed_rule = rule;
  }

  std::vector<const flat_rule::UrlRule*> flat_rules =
      GetAllRulesFromIndex(index);
  for (const auto* rule : flat_rules) {
    EXPECT_EQ(nullptr, map[rule->id()].url_rule.get());
    map[rule->id()].url_rule = rule;
  }

  // Iterate over the map and verify equality of the two representations.
  for (const auto& elem : map) {
    EXPECT_TRUE(AreRulesEqual(elem.second.indexed_rule, elem.second.url_rule))
        << base::StringPrintf("Rule with id %u was incorrectly indexed",
                              elem.first);
  }
}

// Verifies that |extension_metadata| is sorted by ID and corresponds to rules
// in |rules|.
void VerifyExtensionMetadata(
    const std::vector<const IndexedRule*>& rules,
    const ::flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>*
        extension_metdata) {
  struct MetadataPair {
    raw_ptr<const IndexedRule> indexed_rule = nullptr;
    raw_ptr<const flat::UrlRuleMetadata> metadata = nullptr;
  };

  // Build a map from IDs to MetadataPair(s).
  std::map<uint32_t, MetadataPair> map;

  for (const auto* rule : rules) {
    // It is possible for a rule to be present in multiple indices, such as a
    // remove headers rule that removes more than one header.
    EXPECT_TRUE(map[rule->id].indexed_rule == nullptr ||
                map[rule->id].indexed_rule == rule);
    map[rule->id].indexed_rule = rule;
  }

  int previous_id = kMinValidID - 1;
  for (const auto* metadata : *extension_metdata) {
    EXPECT_EQ(nullptr, map[metadata->id()].metadata.get());
    map[metadata->id()].metadata = metadata;

    // Also verify that the metadata vector is sorted by ID.
    int current_id = static_cast<int>(metadata->id());
    EXPECT_LT(previous_id, current_id)
        << "|extension_metdata| is not sorted by ID";
    previous_id = current_id;
  }

  // Returns whether the metadata for the given rule was correctly indexed.
  auto is_metadata_correct = [](const MetadataPair& pair) {
    EXPECT_TRUE(pair.indexed_rule);

    if (ConvertToFlatActionType(pair.indexed_rule->action_type) !=
        pair.metadata->action()) {
      return false;
    }

    EXPECT_FALSE(pair.indexed_rule->redirect_url &&
                 pair.indexed_rule->url_transform);

    if (pair.indexed_rule->redirect_url) {
      if (!pair.metadata->redirect_url()) {
        return false;
      }
      return pair.indexed_rule->redirect_url ==
             ToString(pair.metadata->redirect_url());
    }

    if (pair.indexed_rule->url_transform) {
      if (!pair.metadata->transform()) {
        return false;
      }
      return VerifyUrlTransform(*pair.metadata->transform());
    }

    auto are_header_modifications_equal =
        [](const ::flatbuffers::Vector<
               ::flatbuffers::Offset<flat::ModifyHeaderInfo>>* metadata_headers,
           const std::vector<dnr_api::ModifyHeaderInfo>& indexed_headers) {
          return base::ranges::equal(
              indexed_headers, ToVector(metadata_headers), EqualsForTesting);
        };

    EXPECT_TRUE(are_header_modifications_equal(
        pair.metadata->request_headers(),
        pair.indexed_rule->request_headers_to_modify));
    EXPECT_TRUE(are_header_modifications_equal(
        pair.metadata->response_headers(),
        pair.indexed_rule->response_headers_to_modify));

    return true;
  };

  // Iterate over the map and verify correctness of the metadata.
  for (const auto& elem : map) {
    EXPECT_TRUE(is_metadata_correct(elem.second)) << base::StringPrintf(
        "Redirect rule with id %u was incorrectly indexed", elem.first);
  }
}

const flat::ExtensionIndexedRuleset* AddRuleAndGetRuleset(
    const std::vector<IndexedRule>& rules_to_index,
    flatbuffers::DetachedBuffer* buffer) {
  FlatRulesetIndexer indexer;
  for (const auto& rule : rules_to_index) {
    indexer.AddUrlRule(rule);
  }
  *buffer = indexer.FinishAndReleaseBuffer();

  EXPECT_EQ(rules_to_index.size(), indexer.indexed_rules_count());
  flatbuffers::Verifier verifier(buffer->data(), buffer->size());
  if (!flat::VerifyExtensionIndexedRulesetBuffer(verifier)) {
    return nullptr;
  }

  return flat::GetExtensionIndexedRuleset(buffer->data());
}

// Helper which:
//    - Constructs an ExtensionIndexedRuleset flatbuffer from the passed
//      IndexedRule(s) using FlatRulesetIndexer.
//    - Verifies that the ExtensionIndexedRuleset created is valid.
// Note: this does not test regex rules which are part of the
// ExtensionIndexedRuleset.
void AddRulesAndVerifyIndex(
    const std::vector<IndexedRule>& rules_to_index,
    const std::vector<const IndexedRule*>
        before_request_expected_index_lists[flat::IndexType_count],
    const std::vector<const IndexedRule*>
        headers_received_expected_index_lists[flat::IndexType_count]) {
  flatbuffers::DetachedBuffer buffer;
  const flat::ExtensionIndexedRuleset* ruleset =
      AddRuleAndGetRuleset(rules_to_index, &buffer);
  ASSERT_TRUE(ruleset);

  for (size_t i = 0; i < flat::IndexType_count; ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing index %" PRIuS, i));
    VerifyIndexEquality(before_request_expected_index_lists[i],
                        ruleset->before_request_index_list()->Get(i));
    VerifyIndexEquality(headers_received_expected_index_lists[i],
                        ruleset->headers_received_index_list()->Get(i));
  }

  {
    SCOPED_TRACE("Testing extension metadata");
    std::vector<const IndexedRule*> all_rules;
    for (size_t i = 0; i < flat::IndexType_count; i++) {
      all_rules.insert(all_rules.end(),
                       before_request_expected_index_lists[i].begin(),
                       before_request_expected_index_lists[i].end());
      all_rules.insert(all_rules.end(),
                       headers_received_expected_index_lists[i].begin(),
                       headers_received_expected_index_lists[i].end());
    }
    VerifyExtensionMetadata(all_rules, ruleset->extension_metadata());
  }
}

TEST_F(FlatRulesetIndexerTest, TestEmptyIndex) {
  std::vector<const IndexedRule*>
      expected_before_request_index_lists[flat::IndexType_count];
  std::vector<const IndexedRule*>
      expected_headers_received_index_lists[flat::IndexType_count];
  AddRulesAndVerifyIndex({}, expected_before_request_index_lists,
                         expected_headers_received_index_lists);
}

TEST_F(FlatRulesetIndexerTest, MultipleRules) {
  std::vector<IndexedRule> rules_to_index;

  // Explicitly push the elements instead of using the initializer list
  // constructor, because it does not support move-only types.

  // Blocking rules.
  rules_to_index.push_back(CreateIndexedRule(
      7, kMinValidPriority, flat_rule::OptionFlag_NONE,
      flat_rule::ElementType_OBJECT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_BOUNDARY, "google.com", {"a.com"}, {"x.a.com"},
      std::nullopt, dnr_api::RuleActionType::kBlock, std::nullopt, std::nullopt,
      {}, {}, {}, {}));
  rules_to_index.push_back(CreateIndexedRule(
      2, kMinValidPriority, flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY,
      flat_rule::ElementType_IMAGE | flat_rule::ElementType_WEBSOCKET,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_WILDCARDED,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "*google*",
      {"a.com"}, {}, std::nullopt, dnr_api::RuleActionType::kBlock,
      std::nullopt, std::nullopt, {}, {}, {}, {}));

  // Redirect rules.
  rules_to_index.push_back(CreateIndexedRule(
      15, 2, flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY,
      flat_rule::ElementType_IMAGE, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
      flat_rule::AnchorType_BOUNDARY, "google.com", {}, {},
      "http://example1.com", dnr_api::RuleActionType::kRedirect, std::nullopt,
      std::nullopt, {}, {}, {}, {}));
  rules_to_index.push_back(CreateIndexedRule(
      10, 2, flat_rule::OptionFlag_NONE,
      flat_rule::ElementType_SUBDOCUMENT | flat_rule::ElementType_SCRIPT,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_SUBSTRING,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "example1", {},
      {"a.com"}, "http://example2.com", dnr_api::RuleActionType::kRedirect,
      std::nullopt, std::nullopt, {}, {}, {}, {}));
  rules_to_index.push_back(CreateIndexedRule(
      9, 3, flat_rule::OptionFlag_NONE, flat_rule::ElementType_NONE,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_WILDCARDED,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "*", {}, {},
      "http://example2.com", dnr_api::RuleActionType::kRedirect, std::nullopt,
      std::nullopt, {}, {}, {}, {}));
  rules_to_index.push_back(CreateIndexedRule(
      100, 3, flat_rule::OptionFlag_NONE, flat_rule::ElementType_NONE,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_WILDCARDED,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "*", {}, {},
      std::nullopt, dnr_api::RuleActionType::kRedirect, CreateUrlTransform(),
      std::nullopt, {}, {}, {}, {}));

  // Allow rules.
  rules_to_index.push_back(CreateIndexedRule(
      17, kMinValidPriority, flat_rule::OptionFlag_IS_ALLOWLIST,
      flat_rule::ElementType_PING | flat_rule::ElementType_SCRIPT,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_SUBSTRING,
      flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_NONE,
      "example1.com", {"xyz.com"}, {}, std::nullopt,
      dnr_api::RuleActionType::kAllow, std::nullopt, std::nullopt, {}, {}, {},
      {}));
  rules_to_index.push_back(CreateIndexedRule(
      16, kMinValidPriority,
      flat_rule::OptionFlag_IS_ALLOWLIST |
          flat_rule::OptionFlag_IS_CASE_INSENSITIVE,
      flat_rule::ElementType_IMAGE, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, "example3", {}, {}, std::nullopt,
      dnr_api::RuleActionType::kAllow, std::nullopt, std::nullopt, {}, {}, {},
      {}));

  // Allow all requests rule.
  rules_to_index.push_back(CreateIndexedRule(
      22, 3, flat_rule::OptionFlag_NONE, flat_rule::ElementType_SUBDOCUMENT,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_SUBSTRING,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "example.com", {},
      {}, std::nullopt, dnr_api::RuleActionType::kAllowAllRequests,
      std::nullopt, std::nullopt, {}, {}, {}, {}));

  // Modify headers rules.
  std::vector<dnr_api::ModifyHeaderInfo> request_headers_1;
  request_headers_1.push_back(CreateModifyHeaderInfo(
      dnr_api::HeaderOperation::kSet, "cookie", "sample-cookie"));

  std::vector<dnr_api::ModifyHeaderInfo> response_headers_1;
  response_headers_1.push_back(CreateModifyHeaderInfo(
      dnr_api::HeaderOperation::kRemove, "set-cookie", std::nullopt));

  response_headers_1.push_back(CreateModifyHeaderInfo(
      dnr_api::HeaderOperation::kAppend, "custom-1", "value-1"));

  response_headers_1.push_back(CreateModifyHeaderInfo(
      dnr_api::HeaderOperation::kSet, "custom-2", "value-2"));

  rules_to_index.push_back(CreateIndexedRule(
      23, kMinValidPriority, flat_rule::OptionFlag_IS_CASE_INSENSITIVE,
      flat_rule::ElementType_SUBDOCUMENT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
      flat_rule::AnchorType_NONE, "example.com", {}, {}, std::nullopt,
      dnr_api::RuleActionType::kModifyHeaders, std::nullopt, std::nullopt,
      std::move(request_headers_1), std::move(response_headers_1), {}, {}));

  std::vector<dnr_api::ModifyHeaderInfo> request_headers_2;
  request_headers_2.push_back(CreateModifyHeaderInfo(
      dnr_api::HeaderOperation::kRemove, "referer", std::nullopt));

  request_headers_2.push_back(
      CreateModifyHeaderInfo(dnr_api::HeaderOperation::kRemove, "cookie",
                             std::nullopt, "bad-cookie", std::nullopt));

  {
    std::optional<dnr_api::HeaderRegexOptions> regex_options =
        std::make_optional(dnr_api::HeaderRegexOptions());
    regex_options->match_all = true;

    // TODO(crbug.com/352093575): Make the header operation null when feature
    // launches to avoid the documentation labelling it as optional when all
    // current ModifyHeader actions require an operation to be specified.
    request_headers_2.push_back(CreateModifyHeaderInfo(
        dnr_api::HeaderOperation::kRemove, "cookie", std::nullopt,
        "worst-cookie", "best-cookie=phew", std::move(regex_options)));
  }

  rules_to_index.push_back(CreateIndexedRule(
      24, kMinValidPriority, flat_rule::OptionFlag_IS_CASE_INSENSITIVE,
      flat_rule::ElementType_SUBDOCUMENT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
      flat_rule::AnchorType_NONE, "example.com", {}, {}, std::nullopt,
      dnr_api::RuleActionType::kModifyHeaders, std::nullopt, std::nullopt,
      std::move(request_headers_2), {}, {}, {}));

  {
    // Blocking rule matching on response headers.
    std::vector<dnr_api::HeaderInfo> response_headers;
    response_headers.push_back(CreateHeaderInfo(
        "header", std::vector<std::string>({"value"}), std::nullopt));
    rules_to_index.push_back(CreateIndexedRule(
        137, kMinValidPriority, flat_rule::OptionFlag_NONE,
        flat_rule::ElementType_OBJECT, flat_rule::ActivationType_NONE,
        flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
        flat_rule::AnchorType_BOUNDARY, "google.com", {"a.com"}, {"x.a.com"},
        std::nullopt, dnr_api::RuleActionType::kBlock, std::nullopt,
        std::nullopt, {}, {}, std::move(response_headers), {}));

    // Allow all requests rule matching on excluded response headers.
    std::vector<dnr_api::HeaderInfo> excluded_response_headers;
    excluded_response_headers.push_back(CreateHeaderInfo(
        "excluded-header", std::vector<std::string>({"value"}), std::nullopt));
    rules_to_index.push_back(CreateIndexedRule(
        122, 3, flat_rule::OptionFlag_NONE, flat_rule::ElementType_SUBDOCUMENT,
        flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_SUBSTRING,
        flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "example.com",
        {}, {}, std::nullopt, dnr_api::RuleActionType::kAllowAllRequests,
        std::nullopt, std::nullopt, {}, {}, {},
        std::move(excluded_response_headers)));
  }

  // Note: It's unsafe to store/return pointers to a mutable vector since the
  // vector can resize/reallocate invalidating the existing pointers/iterators.
  // Hence we build `expected_before_request_index_lists` and
  // `expected_headers_received_index_lists` once the vector `rules_to_index` is
  // finalized.
  std::vector<const IndexedRule*>
      expected_before_request_index_lists[flat::IndexType_count];
  expected_before_request_index_lists
      [flat::IndexType_before_request_except_allow_all_requests] = {
          &rules_to_index[0], &rules_to_index[1], &rules_to_index[2],
          &rules_to_index[3], &rules_to_index[4], &rules_to_index[5],
          &rules_to_index[6], &rules_to_index[7]};

  expected_before_request_index_lists[flat::IndexType_allow_all_requests] = {
      &rules_to_index[8]};

  expected_before_request_index_lists[flat::IndexType_modify_headers] = {
      &rules_to_index[9], &rules_to_index[10]};

  std::vector<const IndexedRule*>
      expected_headers_received_index_lists[flat::IndexType_count];
  expected_headers_received_index_lists
      [flat::IndexType_before_request_except_allow_all_requests] = {
          &rules_to_index[11]};
  expected_headers_received_index_lists[flat::IndexType_allow_all_requests] = {
      &rules_to_index[12]};
  expected_headers_received_index_lists[flat::IndexType_modify_headers] = {};

  AddRulesAndVerifyIndex(rules_to_index, expected_before_request_index_lists,
                         expected_headers_received_index_lists);
}

// Verify that the serialized flatbuffer data is valid for regex rules.
TEST_F(FlatRulesetIndexerTest, RegexRules) {
  std::vector<IndexedRule> rules_to_index;

  // Blocking rule.
  rules_to_index.push_back(CreateIndexedRule(
      7, kMinValidPriority, flat_rule::OptionFlag_NONE,
      flat_rule::ElementType_OBJECT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_REGEXP, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, R"(^https://(abc|def))", {"a.com"},
      {"x.a.com"}, std::nullopt, dnr_api::RuleActionType::kBlock, std::nullopt,
      std::nullopt, {}, {}, {}, {}));
  // Redirect rule.
  rules_to_index.push_back(CreateIndexedRule(
      15, 2, flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY,
      flat_rule::ElementType_IMAGE, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_REGEXP, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, R"(^(http|https))", {}, {},
      "http://example1.com", dnr_api::RuleActionType::kRedirect, std::nullopt,
      std::nullopt, {}, {}, {}, {}));
  // Regex substitution rule.
  rules_to_index.push_back(CreateIndexedRule(
      10, 29, flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY,
      flat_rule::ElementType_SCRIPT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_REGEXP, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, R"((\d+\).google.com)", {}, {}, std::nullopt,
      dnr_api::RuleActionType::kRedirect, std::nullopt,
      R"(http://redirect.com?num=\1)", {}, {}, {}, {}));

  // Modify headers rule.
  std::vector<dnr_api::ModifyHeaderInfo> request_headers;
  request_headers.push_back(CreateModifyHeaderInfo(
      dnr_api::HeaderOperation::kRemove, "referer", std::nullopt));
  request_headers.push_back(CreateModifyHeaderInfo(
      dnr_api::HeaderOperation::kSet, "cookie", "sample-cookie"));
  rules_to_index.push_back(CreateIndexedRule(
      21, kMinValidPriority, flat_rule::OptionFlag_IS_CASE_INSENSITIVE,
      flat_rule::ElementType_SUBDOCUMENT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_REGEXP, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, "*", {}, {}, std::nullopt,
      dnr_api::RuleActionType::kModifyHeaders, std::nullopt, std::nullopt,
      std::move(request_headers), {}, {}, {}));

  // Blocking rule that matches on response headers.
  std::vector<dnr_api::HeaderInfo> excluded_response_headers;
  excluded_response_headers.push_back(
      CreateHeaderInfo("excluded-header", std::nullopt, std::nullopt));
  rules_to_index.push_back(CreateIndexedRule(
      117, kMinValidPriority, flat_rule::OptionFlag_NONE,
      flat_rule::ElementType_OBJECT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_REGEXP, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, R"(^https://(abc|def))", {"a.com"},
      {"x.a.com"}, std::nullopt, dnr_api::RuleActionType::kBlock, std::nullopt,
      std::nullopt, {}, {}, {}, std::move(excluded_response_headers)));

  flatbuffers::DetachedBuffer buffer;
  const flat::ExtensionIndexedRuleset* ruleset =
      AddRuleAndGetRuleset(rules_to_index, &buffer);
  ASSERT_TRUE(ruleset);

  // All the indices should be empty, since we only have regex rules.
  for (size_t i = 0; i < flat::IndexType_count; ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing index %" PRIuS, i));
    VerifyIndexEquality({}, ruleset->before_request_index_list()->Get(i));
    VerifyIndexEquality({}, ruleset->headers_received_index_list()->Get(i));
  }

  {
    SCOPED_TRACE("Testing extension metadata");
    std::vector<const IndexedRule*> all_rules;
    for (const IndexedRule& rule : rules_to_index) {
      all_rules.push_back(&rule);
    }
    VerifyExtensionMetadata(all_rules, ruleset->extension_metadata());
  }

  ASSERT_TRUE(ruleset->before_request_regex_rules());
  ASSERT_EQ(4u, ruleset->before_request_regex_rules()->size());

  const flat::RegexRule* blocking_rule = nullptr;
  const flat::RegexRule* redirect_rule = nullptr;
  const flat::RegexRule* regex_substitution_rule = nullptr;
  const flat::RegexRule* modify_header_rule = nullptr;
  for (const auto* regex_rule : *ruleset->before_request_regex_rules()) {
    if (regex_rule->action_type() == flat::ActionType_block) {
      blocking_rule = regex_rule;
    } else if (regex_rule->action_type() == flat::ActionType_redirect) {
      if (regex_rule->regex_substitution()) {
        regex_substitution_rule = regex_rule;
      } else {
        redirect_rule = regex_rule;
      }
    } else if (regex_rule->action_type() == flat::ActionType_modify_headers) {
      modify_header_rule = regex_rule;
    }
  }

  ASSERT_TRUE(blocking_rule);
  EXPECT_TRUE(AreRulesEqual(&rules_to_index[0], blocking_rule->url_rule()));
  EXPECT_FALSE(blocking_rule->regex_substitution());

  ASSERT_TRUE(redirect_rule);
  EXPECT_TRUE(AreRulesEqual(&rules_to_index[1], redirect_rule->url_rule()));
  EXPECT_FALSE(redirect_rule->regex_substitution());

  ASSERT_TRUE(regex_substitution_rule);
  EXPECT_TRUE(
      AreRulesEqual(&rules_to_index[2], regex_substitution_rule->url_rule()));
  EXPECT_EQ(R"(http://redirect.com?num=\1)",
            ToString(regex_substitution_rule->regex_substitution()));

  ASSERT_TRUE(modify_header_rule);
  EXPECT_TRUE(
      AreRulesEqual(&rules_to_index[3], modify_header_rule->url_rule()));
  EXPECT_FALSE(modify_header_rule->regex_substitution());

  ASSERT_TRUE(ruleset->headers_received_regex_rules());
  ASSERT_EQ(1u, ruleset->headers_received_regex_rules()->size());
  const flat::RegexRule* header_matching_rule =
      ruleset->headers_received_regex_rules()->Get(0);

  ASSERT_TRUE(header_matching_rule);
  EXPECT_TRUE(
      AreRulesEqual(&rules_to_index[4], header_matching_rule->url_rule()));
  EXPECT_FALSE(header_matching_rule->regex_substitution());
}

}  // namespace
}  // namespace extensions::declarative_net_request

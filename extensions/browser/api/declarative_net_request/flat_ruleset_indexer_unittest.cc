// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"

#include <stdint.h>
#include <map>
#include <string>

#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;
using FlatRulesetIndexerTest = ::testing::Test;

// Helper to convert a flatbuffer string to a std::string.
std::string ToString(const flatbuffers::String* string) {
  DCHECK(string);
  return std::string(string->c_str(), string->size());
}

// Helper to convert a flatbuffer vector of strings to a std::vector.
std::vector<std::string> ToVector(
    const ::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>*
        vec) {
  if (!vec)
    return std::vector<std::string>();
  std::vector<std::string> result;
  result.reserve(vec->size());
  for (auto* str : *vec)
    result.push_back(ToString(str));
  return result;
}

// Helper to create a generic URLTransform.
std::unique_ptr<dnr_api::URLTransform> CreateUrlTransform() {
  const char* transform = R"(
    {
      "scheme" : "http",
      "host" : "foo.com",
      "port" : "80",
      "path" : "",
      "queryTransform" : {
        "removeParams" : ["x1", "x2"],
        "addOrReplaceParams" : [
          {"key": "y1", "value" : "foo"}
        ]
      },
      "fragment" : "#xx",
      "username" : "user",
      "password" : "pass"
    }
  )";

  base::Optional<base::Value> value = base::JSONReader::Read(transform);
  CHECK(value);

  base::string16 error;
  auto result = dnr_api::URLTransform::FromValue(*value, &error);
  CHECK(result);
  CHECK(error.empty());
  return result;
}

// Helper to verify the indexed form of URlTransform created by
// |CreateUrlTransform()|.
bool VerifyUrlTransform(const flat::UrlTransform& flat_transform) {
  auto is_string_equal = [](base::StringPiece str,
                            const flatbuffers::String* flat_str) {
    return flat_str && ToString(flat_str) == str;
  };

  auto verify_add_or_replace_params = [&flat_transform, &is_string_equal]() {
    if (!flat_transform.add_or_replace_query_params() ||
        flat_transform.add_or_replace_query_params()->size() != 1) {
      return false;
    }

    const flat::QueryKeyValue* query_pair =
        flat_transform.add_or_replace_query_params()->Get(0);
    return query_pair && is_string_equal("y1", query_pair->key()) &&
           is_string_equal("foo", query_pair->value());
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
    base::Optional<std::string> redirect_url,
    dnr_api::RuleActionType action_type,
    std::set<dnr_api::RemoveHeaderType> remove_headers_set,
    std::unique_ptr<dnr_api::URLTransform> url_transform = nullptr) {
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
  rule.domains = std::move(domains);
  rule.excluded_domains = std::move(excluded_domains);
  rule.redirect_url = std::move(redirect_url);
  rule.action_type = action_type;
  rule.remove_headers_set = std::move(remove_headers_set);
  rule.url_transform = std::move(url_transform);
  return rule;
}

// Compares |indexed_rule| and |rule| for equality. Ignores the redirect url
// since it's not stored as part of flat_rule::UrlRule.
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
         indexed_rule->domains == ToVector(rule->domains_included()) &&
         indexed_rule->excluded_domains == ToVector(rule->domains_excluded());
}

// Returns all UrlRule(s) in the given |index|.
std::vector<const flat_rule::UrlRule*> GetAllRulesFromIndex(
    const flat_rule::UrlPatternIndex* index) {
  std::vector<const flat_rule::UrlRule*> result;

  // Iterate over all ngrams and add their corresponding rules.
  for (auto* ngram_to_rules : *index->ngram_index()) {
    if (ngram_to_rules == index->ngram_index_empty_slot())
      continue;
    for (const auto* rule : *ngram_to_rules->rule_list())
      result.push_back(rule);
  }

  // Add all fallback rules.
  for (const auto* rule : *index->fallback_rules())
    result.push_back(rule);

  return result;
}

// Verifies that both |rules| and |index| correspond to the same set of rules
// (in different representations).
void VerifyIndexEquality(const std::vector<const IndexedRule*>& rules,
                         const flat_rule::UrlPatternIndex* index) {
  struct RulePair {
    const IndexedRule* indexed_rule = nullptr;
    const flat_rule::UrlRule* url_rule = nullptr;
  };

  // Build a map from rule IDs to RulePair(s).
  std::map<uint32_t, RulePair> map;

  for (const auto* rule : rules) {
    EXPECT_EQ(nullptr, map[rule->id].indexed_rule);
    map[rule->id].indexed_rule = rule;
  }

  std::vector<const flat_rule::UrlRule*> flat_rules =
      GetAllRulesFromIndex(index);
  for (const auto* rule : flat_rules) {
    EXPECT_EQ(nullptr, map[rule->id()].url_rule);
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
// in |redirect_rules|.
void VerifyExtensionMetadata(
    const std::vector<const IndexedRule*>& redirect_rules,
    const ::flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>*
        extension_metdata) {
  struct MetadataPair {
    const IndexedRule* indexed_rule = nullptr;
    const flat::UrlRuleMetadata* metadata = nullptr;
  };

  // Build a map from IDs to MetadataPair(s).
  std::map<uint32_t, MetadataPair> map;

  for (const auto* rule : redirect_rules) {
    EXPECT_EQ(nullptr, map[rule->id].indexed_rule);
    map[rule->id].indexed_rule = rule;
  }

  int previous_id = kMinValidID - 1;
  for (const auto* metadata : *extension_metdata) {
    EXPECT_EQ(nullptr, map[metadata->id()].metadata);
    map[metadata->id()].metadata = metadata;

    // Also verify that the metadata vector is sorted by ID.
    int current_id = static_cast<int>(metadata->id());
    EXPECT_LT(previous_id, current_id)
        << "|extension_metdata| is not sorted by ID";
    previous_id = current_id;
  }

  // Returns whether the metadata for the given rule was correctly indexed.
  auto is_metadata_correct = [](const MetadataPair& pair) {
    CHECK(pair.indexed_rule->redirect_url || pair.indexed_rule->url_transform);

    if (pair.indexed_rule->redirect_url) {
      if (!pair.metadata->redirect_url())
        return false;
      return pair.indexed_rule->redirect_url ==
             ToString(pair.metadata->redirect_url());
    }

    return pair.metadata->transform() &&
           VerifyUrlTransform(*pair.metadata->transform());
  };

  // Iterate over the map and verify equality of the redirect rules.
  for (const auto& elem : map) {
    EXPECT_TRUE(is_metadata_correct(elem.second)) << base::StringPrintf(
        "Redirect rule with id %u was incorrectly indexed", elem.first);
  }
}

const flat::ExtensionIndexedRuleset* AddRuleAndGetRuleset(
    const std::vector<IndexedRule>& rules_to_index,
    FlatRulesetIndexer* indexer) {
  for (const auto& rule : rules_to_index)
    indexer->AddUrlRule(rule);
  indexer->Finish();

  base::span<const uint8_t> data = indexer->GetData();
  EXPECT_EQ(rules_to_index.size(), indexer->indexed_rules_count());
  flatbuffers::Verifier verifier(data.data(), data.size());
  if (!flat::VerifyExtensionIndexedRulesetBuffer(verifier))
    return nullptr;

  return flat::GetExtensionIndexedRuleset(data.data());
}

// Helper which:
//    - Constructs an ExtensionIndexedRuleset flatbuffer from the passed
//      IndexedRule(s) using FlatRulesetIndexer.
//    - Verifies that the ExtensionIndexedRuleset created is valid.
// Note: this does not test regex rules which are part of the
// ExtensionIndexedRuleset.
void AddRulesAndVerifyIndex(const std::vector<IndexedRule>& rules_to_index,
                            const std::vector<const IndexedRule*>
                                expected_index_lists[flat::ActionIndex_count]) {
  FlatRulesetIndexer indexer;
  const flat::ExtensionIndexedRuleset* ruleset =
      AddRuleAndGetRuleset(rules_to_index, &indexer);
  ASSERT_TRUE(ruleset);

  for (size_t i = 0; i < flat::ActionIndex_count; ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing index %" PRIuS, i));
    VerifyIndexEquality(expected_index_lists[i], ruleset->index_list()->Get(i));
  }

  {
    SCOPED_TRACE("Testing extension metadata");
    VerifyExtensionMetadata(expected_index_lists[flat::ActionIndex_redirect],
                            ruleset->extension_metadata());
  }
}

TEST_F(FlatRulesetIndexerTest, TestEmptyIndex) {
  std::vector<const IndexedRule*> expected_index_lists[flat::ActionIndex_count];
  AddRulesAndVerifyIndex({}, expected_index_lists);
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
      base::nullopt, dnr_api::RULE_ACTION_TYPE_BLOCK, {}));
  rules_to_index.push_back(CreateIndexedRule(
      2, kMinValidPriority, flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY,
      flat_rule::ElementType_IMAGE | flat_rule::ElementType_WEBSOCKET,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_WILDCARDED,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "*google*",
      {"a.com"}, {}, base::nullopt, dnr_api::RULE_ACTION_TYPE_BLOCK, {}));

  // Redirect rules.
  rules_to_index.push_back(CreateIndexedRule(
      15, 2, flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY,
      flat_rule::ElementType_IMAGE, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
      flat_rule::AnchorType_BOUNDARY, "google.com", {}, {},
      "http://example1.com", dnr_api::RULE_ACTION_TYPE_REDIRECT, {}));
  rules_to_index.push_back(CreateIndexedRule(
      10, 2, flat_rule::OptionFlag_NONE,
      flat_rule::ElementType_SUBDOCUMENT | flat_rule::ElementType_SCRIPT,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_SUBSTRING,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "example1", {},
      {"a.com"}, "http://example2.com", dnr_api::RULE_ACTION_TYPE_REDIRECT,
      {}));
  rules_to_index.push_back(CreateIndexedRule(
      9, 3, flat_rule::OptionFlag_NONE, flat_rule::ElementType_NONE,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_WILDCARDED,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "*", {}, {},
      "http://example2.com", dnr_api::RULE_ACTION_TYPE_REDIRECT, {}));
  rules_to_index.push_back(CreateIndexedRule(
      100, 3, flat_rule::OptionFlag_NONE, flat_rule::ElementType_NONE,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_WILDCARDED,
      flat_rule::AnchorType_NONE, flat_rule::AnchorType_NONE, "*", {}, {},
      base::nullopt, dnr_api::RULE_ACTION_TYPE_REDIRECT, {},
      CreateUrlTransform()));

  // Allow rules.
  rules_to_index.push_back(CreateIndexedRule(
      17, kMinValidPriority, flat_rule::OptionFlag_IS_WHITELIST,
      flat_rule::ElementType_PING | flat_rule::ElementType_SCRIPT,
      flat_rule::ActivationType_NONE, flat_rule::UrlPatternType_SUBSTRING,
      flat_rule::AnchorType_SUBDOMAIN, flat_rule::AnchorType_NONE,
      "example1.com", {"xyz.com"}, {}, base::nullopt,
      dnr_api::RULE_ACTION_TYPE_ALLOW, {}));
  rules_to_index.push_back(CreateIndexedRule(
      16, kMinValidPriority,
      flat_rule::OptionFlag_IS_WHITELIST |
          flat_rule::OptionFlag_IS_CASE_INSENSITIVE,
      flat_rule::ElementType_IMAGE, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, "example3", {}, {}, base::nullopt,
      dnr_api::RULE_ACTION_TYPE_ALLOW, {}));

  // Remove request header rules.
  rules_to_index.push_back(CreateIndexedRule(
      20, kMinValidPriority, flat_rule::OptionFlag_IS_CASE_INSENSITIVE,
      flat_rule::ElementType_SUBDOCUMENT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_SUBDOMAIN,
      flat_rule::AnchorType_NONE, "abc", {}, {}, base::nullopt,
      dnr_api::RULE_ACTION_TYPE_REMOVEHEADERS,
      {dnr_api::REMOVE_HEADER_TYPE_COOKIE,
       dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE}));
  rules_to_index.push_back(CreateIndexedRule(
      21, kMinValidPriority, flat_rule::OptionFlag_NONE,
      flat_rule::ElementType_NONE, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_SUBSTRING, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_BOUNDARY, "xyz", {}, {"exclude.com"}, base::nullopt,
      dnr_api::RULE_ACTION_TYPE_REMOVEHEADERS,
      {dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE,
       dnr_api::REMOVE_HEADER_TYPE_COOKIE,
       dnr_api::REMOVE_HEADER_TYPE_REFERER}));

  // Note: It's unsafe to store/return pointers to a mutable vector since the
  // vector can resize/reallocate invalidating the existing pointers/iterators.
  // Hence we build |expected_index_lists| once the vector |rules_to_index| is
  // finalized.
  std::vector<const IndexedRule*> expected_index_lists[flat::ActionIndex_count];
  expected_index_lists[flat::ActionIndex_block] = {&rules_to_index[0],
                                                   &rules_to_index[1]};
  expected_index_lists[flat::ActionIndex_redirect] = {
      &rules_to_index[2], &rules_to_index[3], &rules_to_index[4],
      &rules_to_index[5]};
  expected_index_lists[flat::ActionIndex_allow] = {&rules_to_index[6],
                                                   &rules_to_index[7]};
  expected_index_lists[flat::ActionIndex_remove_cookie_header] = {
      &rules_to_index[8], &rules_to_index[9]};
  expected_index_lists[flat::ActionIndex_remove_referer_header] = {
      &rules_to_index[9]};
  expected_index_lists[flat::ActionIndex_remove_set_cookie_header] = {
      &rules_to_index[8], &rules_to_index[9]};

  AddRulesAndVerifyIndex(rules_to_index, expected_index_lists);
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
      {"x.a.com"}, base::nullopt, dnr_api::RULE_ACTION_TYPE_BLOCK, {}));
  // Redirect rule.
  rules_to_index.push_back(CreateIndexedRule(
      15, 2, flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY,
      flat_rule::ElementType_IMAGE, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_REGEXP, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, R"(^(http|https))", {}, {},
      "http://example1.com", dnr_api::RULE_ACTION_TYPE_REDIRECT, {}));
  // Remove headers rule.
  rules_to_index.push_back(CreateIndexedRule(
      20, kMinValidPriority, flat_rule::OptionFlag_IS_CASE_INSENSITIVE,
      flat_rule::ElementType_SUBDOCUMENT, flat_rule::ActivationType_NONE,
      flat_rule::UrlPatternType_REGEXP, flat_rule::AnchorType_NONE,
      flat_rule::AnchorType_NONE, "*", {}, {}, base::nullopt,
      dnr_api::RULE_ACTION_TYPE_REMOVEHEADERS,
      {dnr_api::REMOVE_HEADER_TYPE_COOKIE,
       dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE}));

  FlatRulesetIndexer indexer;
  const flat::ExtensionIndexedRuleset* ruleset =
      AddRuleAndGetRuleset(rules_to_index, &indexer);
  ASSERT_TRUE(ruleset);

  // All the indices should be empty, since we only have regex rules.
  for (size_t i = 0; i < flat::ActionIndex_count; ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing index %" PRIuS, i));
    VerifyIndexEquality({}, ruleset->index_list()->Get(i));
  }

  // We should have metadata for the redirect rule.
  {
    SCOPED_TRACE("Testing extension metadata");
    VerifyExtensionMetadata({&rules_to_index[1]},
                            ruleset->extension_metadata());
  }

  ASSERT_TRUE(ruleset->regex_rules());
  ASSERT_EQ(3u, ruleset->regex_rules()->size());

  const flat::RegexRule* blocking_rule = nullptr;
  const flat::RegexRule* redirect_rule = nullptr;
  const flat::RegexRule* remove_header_rule = nullptr;
  for (const auto* regex_rule : *ruleset->regex_rules()) {
    if (regex_rule->action_type() == flat::ActionType_block)
      blocking_rule = regex_rule;
    else if (regex_rule->action_type() == flat::ActionType_redirect)
      redirect_rule = regex_rule;
    else if (regex_rule->action_type() == flat::ActionType_remove_headers)
      remove_header_rule = regex_rule;
  }

  ASSERT_TRUE(blocking_rule);
  EXPECT_TRUE(AreRulesEqual(&rules_to_index[0], blocking_rule->url_rule()));
  EXPECT_EQ(0u, blocking_rule->remove_headers_mask());

  ASSERT_TRUE(redirect_rule);
  EXPECT_TRUE(AreRulesEqual(&rules_to_index[1], redirect_rule->url_rule()));
  EXPECT_EQ(0u, redirect_rule->remove_headers_mask());

  ASSERT_TRUE(remove_header_rule);
  EXPECT_TRUE(
      AreRulesEqual(&rules_to_index[2], remove_header_rule->url_rule()));
  EXPECT_EQ(flat::RemoveHeaderType_cookie | flat::RemoveHeaderType_set_cookie,
            remove_header_rule->remove_headers_mask());
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions

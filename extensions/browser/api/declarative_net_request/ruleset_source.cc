// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_source.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "url/gurl.h"

namespace extensions::declarative_net_request {

namespace {

namespace flat_rule = url_pattern_index::flat;

// Returns whether `rule`'s nested `embedder_conditions` buffer, if any, is a
// valid `flat::EmbedderConditions` Flatbuffer.
bool HasValidEmbedderConditions(const flat_rule::UrlRule* rule) {
  const flatbuffers::Vector<uint8_t>* conditions = rule->embedder_conditions();
  if (!conditions) {
    return true;
  }
  flatbuffers::Verifier verifier(conditions->Data(), conditions->size());
  return verifier.VerifyBuffer<flat::EmbedderConditions>(
      kEmbedderConditionsBufferIdentifier);
}

// Returns whether all rules in `ruleset` have valid nested `embedder_conditions`
// buffers. Verifying `ruleset` itself only bounds-checks these byte vectors.
bool AreEmbedderConditionsValid(const flat::ExtensionIndexedRuleset& ruleset) {
  auto all_valid = [](const auto* rules) {
    return !rules || std::ranges::all_of(*rules, &HasValidEmbedderConditions);
  };

  for (const auto* index_list : {ruleset.before_request_index_list(),
                                 ruleset.headers_received_index_list()}) {
    if (!index_list) {
      continue;
    }
    for (const flat_rule::UrlPatternIndex* index : *index_list) {
      if (!all_valid(index->fallback_rules())) {
        return false;
      }
      if (!index->ngram_index()) {
        continue;
      }
      for (const flat_rule::NGramToRules* ngram_to_rules :
           *index->ngram_index()) {
        if (!all_valid(ngram_to_rules->rule_list())) {
          return false;
        }
      }
    }
  }

  for (const auto* regex_rules : {ruleset.before_request_regex_rules(),
                                  ruleset.headers_received_regex_rules()}) {
    if (!regex_rules) {
      continue;
    }
    for (const flat::RegexRule* regex_rule : *regex_rules) {
      if (regex_rule->url_rule() &&
          !HasValidEmbedderConditions(regex_rule->url_rule())) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace

RulesetSource::RulesetSource(RulesetID id,
                             size_t rule_count_limit,
                             ExtensionId extension_id,
                             bool enabled)
    : id_(id),
      rule_count_limit_(rule_count_limit),
      extension_id_(std::move(extension_id)),
      enabled_by_default_(enabled) {}

RulesetSource::~RulesetSource() = default;
RulesetSource::RulesetSource(RulesetSource&&) = default;
RulesetSource& RulesetSource::operator=(RulesetSource&&) = default;

ParseInfo RulesetSource::IndexRules(
    std::vector<api::declarative_net_request::Rule> rules,
    uint8_t parse_flags) const {
  DCHECK_LE(rules.size(), rule_count_limit_);

  // Only warnings or errors can be raised for problematic rules, not both.
  DCHECK(!((parse_flags & ParseFlags::kRaiseErrorOnInvalidRules) &&
           (parse_flags & ParseFlags::kRaiseWarningOnInvalidRules)));
  DCHECK(!((parse_flags & ParseFlags::kRaiseErrorOnLargeRegexRules) &&
           (parse_flags & ParseFlags::kRaiseWarningOnLargeRegexRules)));

  FlatRulesetIndexer indexer;
  std::vector<ParseInfo::RuleWarning> rule_warnings;

  size_t rules_count = 0;
  size_t regex_rules_count = 0;
  {
    std::set<int> id_set;  // Ensure all ids are distinct.
    const GURL base_url = Extension::GetBaseURLFromExtensionId(extension_id_);
    for (auto& rule : rules) {
      int rule_id = rule.id;
      bool inserted = id_set.insert(rule_id).second;
      if (!inserted) {
        if (parse_flags & ParseFlags::kRaiseErrorOnInvalidRules) {
          return ParseInfo(ParseResult::ERROR_DUPLICATE_IDS, rule_id);
        }

        if (parse_flags & ParseFlags::kRaiseWarningOnInvalidRules) {
          rule_warnings.push_back(
              {rule_id,
               GetParseError(ParseResult::ERROR_DUPLICATE_IDS, rule_id)});
        }
        continue;
      }

      // Ignore rules that specify response header matching conditions if the
      // feature is disabled.
      // TODO(crbug.com/40727004): Enable this feature for all versions once
      // initial testing is complete.
      bool has_response_header_conditions =
          rule.condition.response_headers.has_value() ||
          rule.condition.excluded_response_headers.has_value();
      if (has_response_header_conditions &&
          !IsResponseHeaderMatchingEnabled()) {
        continue;
      }

      IndexedRule indexed_rule;
      ParseResult parse_result = IndexedRule::CreateIndexedRule(
          std::move(rule), base_url, id(), &indexed_rule);

      if (parse_result == ParseResult::ERROR_REGEX_TOO_LARGE) {
        if (parse_flags & ParseFlags::kRaiseErrorOnLargeRegexRules) {
          return ParseInfo(parse_result, rule_id);
        }

        if (parse_flags & ParseFlags::kRaiseWarningOnLargeRegexRules) {
          rule_warnings.push_back(
              {rule_id, GetParseError(parse_result, rule_id)});
        }
        continue;
      }

      if (parse_result != ParseResult::SUCCESS) {
        if (parse_flags & ParseFlags::kRaiseErrorOnInvalidRules) {
          return ParseInfo(parse_result, rule_id);
        }

        if (parse_flags & ParseFlags::kRaiseWarningOnInvalidRules) {
          rule_warnings.push_back(
              {rule_id, GetParseError(parse_result, rule_id)});
        }
        continue;
      }

      indexer.AddUrlRule(indexed_rule);
      rules_count++;

      if (indexed_rule.url_pattern_type ==
          url_pattern_index::flat::UrlPatternType_REGEXP) {
        regex_rules_count++;
      }
    }
  }

  flatbuffers::DetachedBuffer buffer = indexer.FinishAndReleaseBuffer();
  int ruleset_checksum = GetChecksum(buffer);
  return ParseInfo(rules_count, regex_rules_count, std::move(rule_warnings),
                   std::move(buffer), ruleset_checksum);
}

LoadRulesetResult RulesetSource::CreateVerifiedMatcher(
    std::string data,
    std::unique_ptr<RulesetMatcher>* matcher) const {
  DCHECK(matcher);

  flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(data.data()),
                                 data.size());

  // TODO(karandeepb): This should use a different LoadRulesetResult since it's
  // not a checksum mismatch.
  // This guarantees that no memory access will end up outside the buffer.
  if (!flat::VerifyExtensionIndexedRulesetBuffer(verifier)) {
    return LoadRulesetResult::kErrorChecksumMismatch;
  }

  // The verification above treats nested `embedder_conditions` buffers as
  // opaque byte vectors, so verify them separately.
  if (!AreEmbedderConditionsValid(
          *flat::GetExtensionIndexedRuleset(data.data()))) {
    return LoadRulesetResult::kErrorChecksumMismatch;
  }

  *matcher =
      std::make_unique<RulesetMatcher>(std::move(data), id(), extension_id());
  return LoadRulesetResult::kSuccess;
}

}  // namespace extensions::declarative_net_request

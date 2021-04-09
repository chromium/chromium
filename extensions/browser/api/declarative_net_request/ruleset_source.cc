// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_source.h"

#include <utility>

#include "base/containers/span.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

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
    std::vector<api::declarative_net_request::Rule> rules) const {
  DCHECK_LE(rules.size(), rule_count_limit_);

  FlatRulesetIndexer indexer;

  size_t rules_count = 0;
  size_t regex_rules_count = 0;
  std::vector<int> large_regex_rule_ids;
  {
    std::set<int> id_set;  // Ensure all ids are distinct.
    const GURL base_url = Extension::GetBaseURLFromExtensionId(extension_id_);
    for (auto& rule : rules) {
      int rule_id = rule.id;
      bool inserted = id_set.insert(rule_id).second;
      if (!inserted)
        return ParseInfo(ParseResult::ERROR_DUPLICATE_IDS, &rule_id);

      IndexedRule indexed_rule;
      ParseResult parse_result = IndexedRule::CreateIndexedRule(
          std::move(rule), base_url, id(), &indexed_rule);

      if (parse_result == ParseResult::ERROR_REGEX_TOO_LARGE) {
        large_regex_rule_ids.push_back(rule_id);
        continue;
      }

      if (parse_result != ParseResult::SUCCESS)
        return ParseInfo(parse_result, &rule_id);

      indexer.AddUrlRule(indexed_rule);
      rules_count++;

      if (indexed_rule.url_pattern_type ==
          url_pattern_index::flat::UrlPatternType_REGEXP) {
        regex_rules_count++;
      }
    }
  }

  flatbuffers::DetachedBuffer buffer = indexer.FinishAndReleaseBuffer();
  int ruleset_checksum =
      GetChecksum(base::make_span(buffer.data(), buffer.size()));
  return ParseInfo(rules_count, regex_rules_count,
                   std::move(large_regex_rule_ids), std::move(buffer),
                   ruleset_checksum);
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
  if (!flat::VerifyExtensionIndexedRulesetBuffer(verifier))
    return LoadRulesetResult::kErrorChecksumMismatch;

  *matcher =
      std::make_unique<RulesetMatcher>(std::move(data), id(), extension_id());
  return LoadRulesetResult::kSuccess;
}

}  // namespace declarative_net_request
}  // namespace extensions

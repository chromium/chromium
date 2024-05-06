// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher_base.h"

namespace extensions::declarative_net_request {

// ExtensionUrlPatternIndexMatcher is an implementation detail of
// RulesetMatcher. It deals with matching of filter list style rules. This uses
// the url_pattern_index component to achieve fast matching of network requests
// against declarative rules.
class ExtensionUrlPatternIndexMatcher final : public RulesetMatcherBase {
 public:
  using UrlPatternIndexList = flatbuffers::Vector<
      flatbuffers::Offset<url_pattern_index::flat::UrlPatternIndex>>;
  ExtensionUrlPatternIndexMatcher(
      const ExtensionId& extension_id,
      RulesetID ruleset_id,
      const UrlPatternIndexList* before_request_index_list,
      const UrlPatternIndexList* headers_received_index_list,
      const ExtensionMetadataList* metadata_list);

  ExtensionUrlPatternIndexMatcher(const ExtensionUrlPatternIndexMatcher&) =
      delete;
  ExtensionUrlPatternIndexMatcher& operator=(
      const ExtensionUrlPatternIndexMatcher&) = delete;

  // RulesetMatcherBase override:
  ~ExtensionUrlPatternIndexMatcher() override;
  std::vector<RequestAction> GetModifyHeadersActions(
      const RequestParams& params,
      RulesetMatchingStage stage,
      std::optional<uint64_t> min_priority) const override;
  bool IsExtraHeadersMatcher() const override;
  size_t GetRulesCount() const override;
  size_t GetBeforeRequestRulesCount() const override;
  size_t GetHeadersReceivedRulesCount() const override;

  // Sets the disabled rule ids so that the disabled rules are not matched.
  void SetDisabledRuleIds(base::flat_set<int> disabled_rule_ids);

  const base::flat_set<int>& GetDisabledRuleIdsForTesting() const;

 private:
  using UrlPatternIndexMatcher = url_pattern_index::UrlPatternIndexMatcher;

  // RulesetMatcherBase override:
  std::optional<RequestAction> GetAllowAllRequestsAction(
      const RequestParams& params,
      RulesetMatchingStage stage) const override;
  std::optional<RequestAction> GetActionIgnoringAncestors(
      const RequestParams& params,
      RulesetMatchingStage stage) const override;

  // Returns the highest priority action from
  // |flat::IndexType_before_request_except_allow_all_requests| index.
  std::optional<RequestAction> GetActionHelper(
      const RequestParams& params,
      const std::vector<UrlPatternIndexMatcher>& matchers) const;

  const url_pattern_index::flat::UrlRule* GetMatchingRule(
      const RequestParams& params,
      const std::vector<UrlPatternIndexMatcher>& matchers,
      flat::IndexType index,
      UrlPatternIndexMatcher::FindRuleStrategy strategy =
          UrlPatternIndexMatcher::FindRuleStrategy::kAny) const;

  std::vector<const url_pattern_index::flat::UrlRule*> GetAllMatchingRules(
      const RequestParams& params,
      const std::vector<UrlPatternIndexMatcher>& matchers,
      flat::IndexType index) const;

  // Returns the corresponding rule matchers for the given rule matching
  // `stage`.
  const std::vector<UrlPatternIndexMatcher>& GetMatchersForStage(
      RulesetMatchingStage stage) const;

  const raw_ptr<const ExtensionMetadataList> metadata_list_;

  // UrlPatternIndexMatchers for rules to be matched in the onBeforeRequest
  // phase corresponding to entries in flat::IndexType.
  const std::vector<UrlPatternIndexMatcher> before_request_matchers_;

  // UrlPatternIndexMatchers for rules to be matched in the onHeadersReceived
  // phase corresponding to entries in flat::IndexType.
  const std::vector<UrlPatternIndexMatcher> headers_received_matchers_;

  const size_t before_request_rules_count_;

  const size_t headers_received_rules_count_;

  // Whether this matcher contains rules that will match on, or modify headers.
  const bool is_extra_headers_matcher_;

  // Disabled rule ids. The ids are passed to the matching algorithm in the
  // UrlPatternIndexMatcher so that the algorithm can skip the disabled rules.
  base::flat_set<int> disabled_rule_ids_;
};

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_EXTENSION_URL_PATTERN_INDEX_MATCHER_H_

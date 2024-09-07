// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/extension_url_pattern_index_matcher.h"

#include <algorithm>
#include <limits>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/common/api/declarative_net_request.h"

namespace extensions::declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

std::vector<url_pattern_index::UrlPatternIndexMatcher> GetMatchers(
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list) {
  DCHECK(index_list);
  DCHECK_EQ(flat::IndexType_count, index_list->size());

  std::vector<url_pattern_index::UrlPatternIndexMatcher> matchers;
  matchers.reserve(flat::IndexType_count);
  for (const flat_rule::UrlPatternIndex* index : *index_list) {
    matchers.emplace_back(index);
  }
  return matchers;
}

bool IsExtraHeadersMatcherInternal(
    const std::vector<url_pattern_index::UrlPatternIndexMatcher>& matchers) {
  static_assert(flat::IndexType_count == 3,
                "Modify this method to ensure IsExtraHeadersMatcherInternal is "
                "updated as new actions are added.");
  return matchers[flat::IndexType_modify_headers].GetRulesCount() > 0;
}

size_t GetRulesCountInternal(
    const std::vector<url_pattern_index::UrlPatternIndexMatcher>& matchers) {
  size_t rules_count = 0;
  for (const auto& matcher : matchers) {
    rules_count += matcher.GetRulesCount();
  }

  return rules_count;
}

}  // namespace

ExtensionUrlPatternIndexMatcher::ExtensionUrlPatternIndexMatcher(
    const ExtensionId& extension_id,
    RulesetID ruleset_id,
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList*
        before_request_index_list,
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList*
        headers_received_index_list,
    const ExtensionMetadataList* metadata_list)
    : RulesetMatcherBase(extension_id, ruleset_id),
      metadata_list_(metadata_list),
      before_request_matchers_(GetMatchers(before_request_index_list)),
      headers_received_matchers_(GetMatchers(headers_received_index_list)),
      before_request_rules_count_(
          GetRulesCountInternal(before_request_matchers_)),
      headers_received_rules_count_(
          GetRulesCountInternal(headers_received_matchers_)),
      // Currently, this is set to true if there exist any rules that match on,
      // or modify headers. This is why we check if there are any rules to be
      // matched in the onHeadersReceived phase since they need to match on
      // response headers.
      // TODO(kelvinjiang): Consider separating this condition for request and
      // response headers so extra headers are only included for the phases
      // that need them.
      is_extra_headers_matcher_(
          IsExtraHeadersMatcherInternal(before_request_matchers_) ||
          headers_received_rules_count_ > 0) {}

ExtensionUrlPatternIndexMatcher::~ExtensionUrlPatternIndexMatcher() = default;

bool ExtensionUrlPatternIndexMatcher::IsExtraHeadersMatcher() const {
  return is_extra_headers_matcher_;
}

size_t ExtensionUrlPatternIndexMatcher::GetRulesCount() const {
  return before_request_rules_count_ + headers_received_rules_count_;
}

size_t ExtensionUrlPatternIndexMatcher::GetBeforeRequestRulesCount() const {
  return before_request_rules_count_;
}

size_t ExtensionUrlPatternIndexMatcher::GetHeadersReceivedRulesCount() const {
  return headers_received_rules_count_;
}

std::optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetAllowAllRequestsAction(
    const RequestParams& params,
    RulesetMatchingStage stage) const {
  const flat_rule::UrlRule* rule = GetMatchingRule(
      params, GetMatchersForStage(stage), flat::IndexType_allow_all_requests,
      FindRuleStrategy::kHighestPriority);
  ;
  if (!rule) {
    return std::nullopt;
  }

  return CreateAllowAllRequestsAction(params, *rule);
}

std::vector<RequestAction>
ExtensionUrlPatternIndexMatcher::GetModifyHeadersActions(
    const RequestParams& params,
    RulesetMatchingStage stage,
    std::optional<uint64_t> min_priority) const {
  // TODO(crbug.com/40131283): Plumb |min_priority| into UrlPatternIndexMatcher
  // to prune more rules before matching on url filters.
  std::vector<const flat_rule::UrlRule*> rules = GetAllMatchingRules(
      params, GetMatchersForStage(stage), flat::IndexType_modify_headers);

  if (min_priority) {
    std::erase_if(rules, [&min_priority](const flat_rule::UrlRule* rule) {
      return rule->priority() <= *min_priority;
    });
  }

  return GetModifyHeadersActionsFromMetadata(params, rules, *metadata_list_);
}

std::optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetActionIgnoringAncestors(
    const RequestParams& params,
    RulesetMatchingStage stage) const {
  return GetMaxPriorityAction(
      GetActionHelper(params, GetMatchersForStage(stage)),
      GetAllowAllRequestsAction(params, stage));
}

std::optional<RequestAction> ExtensionUrlPatternIndexMatcher::GetActionHelper(
    const RequestParams& params,
    const std::vector<UrlPatternIndexMatcher>& matchers) const {
  const flat_rule::UrlRule* rule =
      GetMatchingRule(params, matchers,
                      flat::IndexType_before_request_except_allow_all_requests,
                      FindRuleStrategy::kHighestPriority);
  if (!rule) {
    return std::nullopt;
  }

  const flat::UrlRuleMetadata* metadata =
      metadata_list_->LookupByKey(rule->id());
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule->id());
  switch (metadata->action()) {
    case flat::ActionType_block:
      return CreateBlockOrCollapseRequestAction(params, *rule);
    case flat::ActionType_allow:
      return CreateAllowAction(params, *rule);
    case flat::ActionType_redirect:
      return CreateRedirectActionFromMetadata(params, *rule, *metadata_list_);
    case flat::ActionType_upgrade_scheme:
      return CreateUpgradeAction(params, *rule);
    case flat::ActionType_allow_all_requests:
    case flat::ActionType_modify_headers:
    case flat::ActionType_count:
      NOTREACHED_IN_MIGRATION();
  }

  return std::nullopt;
}

const flat_rule::UrlRule* ExtensionUrlPatternIndexMatcher::GetMatchingRule(
    const RequestParams& params,
    const std::vector<UrlPatternIndexMatcher>& matchers,
    flat::IndexType index,
    FindRuleStrategy strategy) const {
  DCHECK_LT(index, flat::IndexType_count);
  DCHECK_GE(index, 0);
  DCHECK(params.url);

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool kDisableGenericRules = false;

  return matchers[index].FindMatch(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.method, params.is_third_party,
      kDisableGenericRules, params.embedder_conditions_matcher, strategy,
      disabled_rule_ids_);
}

std::vector<const url_pattern_index::flat::UrlRule*>
ExtensionUrlPatternIndexMatcher::GetAllMatchingRules(
    const RequestParams& params,
    const std::vector<UrlPatternIndexMatcher>& matchers,
    flat::IndexType index) const {
  DCHECK_LT(index, flat::IndexType_count);
  DCHECK_GE(index, 0);
  DCHECK(params.url);

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool kDisableGenericRules = false;

  return matchers[index].FindAllMatches(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.method, params.is_third_party,
      kDisableGenericRules, params.embedder_conditions_matcher,
      disabled_rule_ids_);
}

const std::vector<url_pattern_index::UrlPatternIndexMatcher>&
ExtensionUrlPatternIndexMatcher::GetMatchersForStage(
    RulesetMatchingStage stage) const {
  switch (stage) {
    case RulesetMatchingStage::kOnBeforeRequest:
      return before_request_matchers_;
    case RulesetMatchingStage::kOnHeadersReceived:
      return headers_received_matchers_;
  }

  NOTREACHED_IN_MIGRATION();
  return before_request_matchers_;
}

void ExtensionUrlPatternIndexMatcher::SetDisabledRuleIds(
    base::flat_set<int> disabled_rule_ids) {
  disabled_rule_ids_ = std::move(disabled_rule_ids);
  disabled_rule_ids_.shrink_to_fit();
}

const base::flat_set<int>&
ExtensionUrlPatternIndexMatcher::GetDisabledRuleIdsForTesting() const {
  return disabled_rule_ids_;
}

}  // namespace extensions::declarative_net_request

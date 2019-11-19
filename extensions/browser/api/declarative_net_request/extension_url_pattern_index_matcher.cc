// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/extension_url_pattern_index_matcher.h"

#include <algorithm>
#include <limits>
#include <list>
#include <string>
#include <utility>

#include "base/logging.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/common/api/declarative_net_request.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

std::vector<url_pattern_index::UrlPatternIndexMatcher> GetMatchers(
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list) {
  DCHECK(index_list);
  DCHECK_EQ(flat::ActionIndex_count, index_list->size());

  std::vector<url_pattern_index::UrlPatternIndexMatcher> matchers;
  matchers.reserve(flat::ActionIndex_count);
  for (const flat_rule::UrlPatternIndex* index : *index_list)
    matchers.emplace_back(index);
  return matchers;
}

bool HasAnyRules(const url_pattern_index::flat::UrlPatternIndex* index) {
  DCHECK(index);

  if (index->fallback_rules()->size() > 0)
    return true;

  // Iterate over all ngrams and check their corresponding rules.
  for (auto* ngram_to_rules : *index->ngram_index()) {
    if (ngram_to_rules == index->ngram_index_empty_slot())
      continue;

    if (ngram_to_rules->rule_list()->size() > 0)
      return true;
  }

  return false;
}

bool IsExtraHeadersMatcherInternal(
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list) {
  // We only support removing a subset of extra headers currently. If that
  // changes, the implementation here should change as well.
  static_assert(flat::ActionIndex_count == 7,
                "Modify this method to ensure IsExtraHeadersMatcherInternal is "
                "updated as new actions are added.");
  static const flat::ActionIndex extra_header_indices[] = {
      flat::ActionIndex_remove_cookie_header,
      flat::ActionIndex_remove_referer_header,
      flat::ActionIndex_remove_set_cookie_header,
  };

  for (flat::ActionIndex index : extra_header_indices) {
    if (HasAnyRules(index_list->Get(index)))
      return true;
  }

  return false;
}

}  // namespace

ExtensionUrlPatternIndexMatcher::ExtensionUrlPatternIndexMatcher(
    const ExtensionId& extension_id,
    api::declarative_net_request::SourceType source_type,
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list,
    const ExtensionMetadataList* metadata_list)
    : RulesetMatcherInterface(extension_id, source_type),
      metadata_list_(metadata_list),
      matchers_(GetMatchers(index_list)),
      is_extra_headers_matcher_(IsExtraHeadersMatcherInternal(index_list)) {}

ExtensionUrlPatternIndexMatcher::~ExtensionUrlPatternIndexMatcher() = default;

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetBlockOrCollapseAction(
    const RequestParams& params) const {
  const flat_rule::UrlRule* rule =
      GetMatchingRule(params, flat::ActionIndex_block);
  if (!rule)
    return base::nullopt;

  return CreateBlockOrCollapseRequestAction(params, *rule);
}

base::Optional<RequestAction> ExtensionUrlPatternIndexMatcher::GetAllowAction(
    const RequestParams& params) const {
  const flat_rule::UrlRule* rule =
      GetMatchingRule(params, flat::ActionIndex_allow);
  if (!rule)
    return base::nullopt;

  return CreateAllowAction(params, *rule);
}

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetRedirectAction(
    const RequestParams& params) const {
  const flat_rule::UrlRule* redirect_rule = GetMatchingRule(
      params, flat::ActionIndex_redirect, FindRuleStrategy::kHighestPriority);
  if (!redirect_rule)
    return base::nullopt;

  return CreateRedirectAction(params, *redirect_rule, *metadata_list_);
}

base::Optional<RequestAction> ExtensionUrlPatternIndexMatcher::GetUpgradeAction(
    const RequestParams& params) const {
  DCHECK(IsUpgradeableRequest(params));

  const flat_rule::UrlRule* upgrade_rule =
      GetMatchingRule(params, flat::ActionIndex_upgrade_scheme,
                      FindRuleStrategy::kHighestPriority);
  if (!upgrade_rule)
    return base::nullopt;

  return CreateUpgradeAction(params, *upgrade_rule);
}

uint8_t ExtensionUrlPatternIndexMatcher::GetRemoveHeadersMask(
    const RequestParams& params,
    uint8_t ignored_mask,
    std::vector<RequestAction>* remove_headers_actions) const {
  // The same flat_rule::UrlRule may be split across different action indices.
  // To ensure we return one RequestAction for one ID/rule, maintain a map from
  // the rule to the mask of rules removed for that rule.
  base::flat_map<const flat_rule::UrlRule*, uint8_t> rule_to_mask_map;
  auto handle_remove_header_bit = [this, &params, ignored_mask,
                                   &rule_to_mask_map](uint8_t bit,
                                                      flat::ActionIndex index) {
    if (ignored_mask & bit)
      return;

    const flat_rule::UrlRule* rule = GetMatchingRule(params, index);
    if (!rule)
      return;

    rule_to_mask_map[rule] |= bit;
  };

  // Iterate over each RemoveHeaderType value.
  uint8_t bit = 0;
  for (int i = 0; i <= dnr_api::REMOVE_HEADER_TYPE_LAST; ++i) {
    switch (i) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        bit = flat::RemoveHeaderType_cookie;
        handle_remove_header_bit(bit, flat::ActionIndex_remove_cookie_header);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        bit = flat::RemoveHeaderType_referer;
        handle_remove_header_bit(bit, flat::ActionIndex_remove_referer_header);
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        bit = flat::RemoveHeaderType_set_cookie;
        handle_remove_header_bit(bit,
                                 flat::ActionIndex_remove_set_cookie_header);
        break;
    }
  }

  uint8_t mask = 0;
  for (const auto& it : rule_to_mask_map) {
    uint8_t mask_for_rule = it.second;
    DCHECK(mask_for_rule);
    mask |= mask_for_rule;

    remove_headers_actions->push_back(
        GetRemoveHeadersActionForMask(*it.first, mask_for_rule));
  }

  DCHECK(!(mask & ignored_mask));
  return mask;
}

const flat_rule::UrlRule* ExtensionUrlPatternIndexMatcher::GetMatchingRule(
    const RequestParams& params,
    flat::ActionIndex index,
    FindRuleStrategy strategy) const {
  DCHECK_LT(index, flat::ActionIndex_count);
  DCHECK_GE(index, 0);
  DCHECK(params.url);

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool kDisableGenericRules = false;

  return matchers_[index].FindMatch(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.is_third_party,
      kDisableGenericRules, strategy);
}

}  // namespace declarative_net_request
}  // namespace extensions

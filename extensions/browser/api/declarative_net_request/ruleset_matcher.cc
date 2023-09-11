// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rules_count_pair.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {

RulesetMatcher::RulesetMatcher(std::string ruleset_data,
                               RulesetID id,
                               const ExtensionId& extension_id)
    : ruleset_data_(std::move(ruleset_data)),
      root_(flat::GetExtensionIndexedRuleset(ruleset_data_.data())),
      id_(id),
      url_pattern_index_matcher_(extension_id,
                                 id,
                                 root_->index_list(),
                                 root_->extension_metadata()),
      regex_matcher_(extension_id,
                     id,
                     root_->regex_rules(),
                     root_->extension_metadata()) {}

RulesetMatcher::~RulesetMatcher() = default;

absl::optional<RequestAction> RulesetMatcher::GetBeforeRequestAction(
    const RequestParams& params) const {
  return GetMaxPriorityAction(
      url_pattern_index_matcher_.GetBeforeRequestAction(params),
      regex_matcher_.GetBeforeRequestAction(params));
}

std::vector<RequestAction> RulesetMatcher::GetModifyHeadersActions(
    const RequestParams& params,
    absl::optional<uint64_t> min_priority) const {
  std::vector<RequestAction> modify_header_actions =
      url_pattern_index_matcher_.GetModifyHeadersActions(params, min_priority);

  std::vector<RequestAction> regex_modify_header_actions =
      regex_matcher_.GetModifyHeadersActions(params, min_priority);

  modify_header_actions.insert(
      modify_header_actions.end(),
      std::make_move_iterator(regex_modify_header_actions.begin()),
      std::make_move_iterator(regex_modify_header_actions.end()));

  return modify_header_actions;
}

bool RulesetMatcher::IsExtraHeadersMatcher() const {
  return url_pattern_index_matcher_.IsExtraHeadersMatcher() ||
         regex_matcher_.IsExtraHeadersMatcher();
}

size_t RulesetMatcher::GetRulesCount() const {
  return url_pattern_index_matcher_.GetRulesCount() +
         regex_matcher_.GetRulesCount();
}

size_t RulesetMatcher::GetRegexRulesCount() const {
  return regex_matcher_.GetRulesCount();
}

RulesCountPair RulesetMatcher::GetRulesCountPair() const {
  return RulesCountPair(GetRulesCount(), GetRegexRulesCount());
}

void RulesetMatcher::OnRenderFrameCreated(content::RenderFrameHost* host) {
  url_pattern_index_matcher_.OnRenderFrameCreated(host);
  regex_matcher_.OnRenderFrameCreated(host);
}

void RulesetMatcher::OnRenderFrameDeleted(content::RenderFrameHost* host) {
  url_pattern_index_matcher_.OnRenderFrameDeleted(host);
  regex_matcher_.OnRenderFrameDeleted(host);
}

void RulesetMatcher::OnDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  url_pattern_index_matcher_.OnDidFinishNavigation(navigation_handle);
  regex_matcher_.OnDidFinishNavigation(navigation_handle);
}

absl::optional<RequestAction>
RulesetMatcher::GetAllowlistedFrameActionForTesting(
    content::RenderFrameHost* host) const {
  return GetMaxPriorityAction(
      url_pattern_index_matcher_.GetAllowlistedFrameActionForTesting(host),
      regex_matcher_.GetAllowlistedFrameActionForTesting(host));
}

void RulesetMatcher::SetDisabledRuleIds(base::flat_set<int> disabled_rule_ids) {
  url_pattern_index_matcher_.SetDisabledRuleIds(std::move(disabled_rule_ids));
}

const base::flat_set<int>& RulesetMatcher::GetDisabledRuleIdsForTesting()
    const {
  return url_pattern_index_matcher_.GetDisabledRuleIdsForTesting();
}

}  // namespace declarative_net_request
}  // namespace extensions

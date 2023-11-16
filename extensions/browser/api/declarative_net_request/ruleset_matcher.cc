// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <iterator>
#include <optional>
#include <utility>
#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rule_counts.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions::declarative_net_request {

namespace {

using ExtensionMetadataList =
    flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>;

size_t ComputeUnsafeRuleCount(const ExtensionMetadataList* metadata_list) {
  size_t unsafe_rule_count = 0;
  for (const auto* url_rule_metadata : *metadata_list) {
    if (!IsRuleSafe(*url_rule_metadata)) {
      unsafe_rule_count++;
    }
  }
  return unsafe_rule_count;
}

bool IsRulesetStatic(const RulesetID& id) {
  return id != kDynamicRulesetID && id != kSessionRulesetID;
}

}  // namespace

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
                     root_->extension_metadata()) {
  if (!IsRulesetStatic(id)) {
    unsafe_rule_count_ = ComputeUnsafeRuleCount(root_->extension_metadata());
  }
}

RulesetMatcher::~RulesetMatcher() = default;

std::optional<RequestAction> RulesetMatcher::GetBeforeRequestAction(
    const RequestParams& params) const {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::optional<RequestAction> regex_result =
      regex_matcher_.GetBeforeRequestAction(params);
  base::TimeDelta regex_time = base::TimeTicks::Now() - start_time;
  std::optional<RequestAction> url_pattern_result =
      url_pattern_index_matcher_.GetBeforeRequestAction(params);
  std::optional<RequestAction> final_result = GetMaxPriorityAction(
      std::move(url_pattern_result), std::move(regex_result));
  base::TimeDelta total_time = base::TimeTicks::Now() - start_time;
  int regex_rules_count = GetRegexRulesCount();
  int rules_count = GetRulesCount();

  int percent_taken_by_regex = 0;
  // It's possible that the rule evaluation took no measurable time; be sure we
  // don't divide by zero.
  if (regex_time.is_positive()) {
    percent_taken_by_regex =
        static_cast<int>((regex_time / total_time) * 100.0);
  }

  constexpr int kBucketCount = 50;
  constexpr base::TimeDelta kMinTime = base::Microseconds(1);
  constexpr base::TimeDelta kMaxTime = base::Seconds(3);

  if (regex_rules_count > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Extensions.DeclarativeNetRequest.RegexRulesEvaluationPercentage",
        percent_taken_by_regex);

    if (regex_rules_count < 15) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "LessThan15Rules",
          regex_time, kMinTime, kMaxTime, kBucketCount);
    } else if (regex_rules_count < 100) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "15To100Rules",
          regex_time, kMinTime, kMaxTime, kBucketCount);
    } else if (regex_rules_count < 500) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "100To500Rules",
          regex_time, kMinTime, kMaxTime, kBucketCount);
    } else {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "Over500Rules",
          regex_time, kMinTime, kMaxTime, kBucketCount);
    }
  }

  if (rules_count < 1000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "LessThan1000Rules",
        total_time, kMinTime, kMaxTime, kBucketCount);
  } else if (rules_count < 10000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "1000To10000Rules",
        total_time, kMinTime, kMaxTime, kBucketCount);
  } else if (rules_count < 30000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "10000To30000Rules",
        total_time, kMinTime, kMaxTime, kBucketCount);
  } else if (rules_count < 100000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "30000To100000Rules",
        total_time, kMinTime, kMaxTime, kBucketCount);
  } else if (rules_count < 300000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "100000To300000Rules",
        total_time, kMinTime, kMaxTime, kBucketCount);
  } else {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "Over300000Rules",
        total_time, kMinTime, kMaxTime, kBucketCount);
  }

  return final_result;
}

std::vector<RequestAction> RulesetMatcher::GetModifyHeadersActions(
    const RequestParams& params,
    std::optional<uint64_t> min_priority) const {
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

std::optional<size_t> RulesetMatcher::GetUnsafeRulesCount() const {
  return unsafe_rule_count_;
}

size_t RulesetMatcher::GetRegexRulesCount() const {
  return regex_matcher_.GetRulesCount();
}

RuleCounts RulesetMatcher::GetRuleCounts() const {
  return RuleCounts(GetRulesCount(), unsafe_rule_count_, GetRegexRulesCount());
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

std::optional<RequestAction>
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

}  // namespace extensions::declarative_net_request

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <iterator>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rule_counts.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher_base.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions::declarative_net_request {

namespace {

using ExtensionMetadataList =
    flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>;

// These constants specify the number of, the minimum, and the maximum buckets
// for histograms which record the evaluation time for a request against a
// single DNR ruleset.
constexpr int kRequestActionTimeUmaBucketCount = 50;
constexpr base::TimeDelta kRequestActionUmaMinTime = base::Microseconds(1);
constexpr base::TimeDelta kRequestActionUmaMaxTime = base::Seconds(3);

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

void RecordOnBeforeRequestActionTime(const base::TimeTicks& start_time,
                                     const base::TimeDelta& regex_time,
                                     const base::TimeDelta& total_time,
                                     int rules_count,
                                     int regex_rules_count) {
  int percent_taken_by_regex = 0;
  // It's possible that the rule evaluation took no measurable time; be sure we
  // don't divide by zero.
  if (regex_time.is_positive()) {
    percent_taken_by_regex =
        static_cast<int>((regex_time / total_time) * 100.0);
  }

  if (regex_rules_count > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Extensions.DeclarativeNetRequest."
        "RegexRulesBeforeRequestEvaluationPercentage",
        percent_taken_by_regex);

    if (regex_rules_count < 15) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "LessThan15Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    } else if (regex_rules_count < 100) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "15To100Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    } else if (regex_rules_count < 500) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "100To500Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    } else {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest.RegexRulesBeforeRequestActionTime."
          "Over500Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    }
  }

  if (rules_count < 1000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "LessThan1000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 10000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "1000To10000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 30000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "10000To30000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 100000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "30000To100000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 300000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "100000To300000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingBeforeRequestActionTime."
        "Over300000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  }
}

void RecordOnHeadersReceivedActionTime(const base::TimeTicks& start_time,
                                       const base::TimeDelta& regex_time,
                                       const base::TimeDelta& total_time,
                                       int rules_count,
                                       int regex_rules_count) {
  int percent_taken_by_regex = 0;
  // It's possible that the rule evaluation took no measurable time; be sure we
  // don't divide by zero.
  if (regex_time.is_positive()) {
    percent_taken_by_regex =
        static_cast<int>((regex_time / total_time) * 100.0);
  }

  if (regex_rules_count > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Extensions.DeclarativeNetRequest."
        "RegexRulesHeadersReceivedEvaluationPercentage",
        percent_taken_by_regex);

    if (regex_rules_count < 15) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest."
          "RegexRulesHeadersReceivedActionTime."
          "LessThan15Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    } else if (regex_rules_count < 100) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest."
          "RegexRulesHeadersReceivedActionTime."
          "15To100Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    } else if (regex_rules_count < 500) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest."
          "RegexRulesHeadersReceivedActionTime."
          "100To500Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    } else {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Extensions.DeclarativeNetRequest."
          "RegexRulesHeadersReceivedActionTime."
          "Over500Rules",
          regex_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
          kRequestActionTimeUmaBucketCount);
    }
  }

  if (rules_count < 1000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingHeadersReceivedActionTime."
        "LessThan1000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 10000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingHeadersReceivedActionTime."
        "1000To10000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 30000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingHeadersReceivedActionTime."
        "10000To30000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 100000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingHeadersReceivedActionTime."
        "30000To100000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else if (rules_count < 300000) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingHeadersReceivedActionTime."
        "100000To300000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  } else {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest."
        "RulesetMatchingHeadersReceivedActionTime."
        "Over300000Rules",
        total_time, kRequestActionUmaMinTime, kRequestActionUmaMaxTime,
        kRequestActionTimeUmaBucketCount);
  }
}

}  // namespace

RulesetMatcher::RulesetMatcher(std::string ruleset_data,
                               RulesetID id,
                               const ExtensionId& extension_id)
    : ruleset_data_(std::move(ruleset_data)),
      root_(flat::GetExtensionIndexedRuleset(ruleset_data_.data())),
      id_(id),
      url_matcher_(extension_id,
                   id,
                   root_->before_request_index_list(),
                   root_->headers_received_index_list(),
                   root_->extension_metadata()),
      regex_matcher_(extension_id,
                     id,
                     root_->before_request_regex_rules(),
                     root_->headers_received_regex_rules(),
                     root_->extension_metadata()) {
  if (!IsRulesetStatic(id)) {
    unsafe_rule_count_ = ComputeUnsafeRuleCount(root_->extension_metadata());
  }
}

RulesetMatcher::~RulesetMatcher() = default;

std::optional<RequestAction> RulesetMatcher::GetAction(
    const RequestParams& params,
    RulesetMatchingStage stage) const {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::optional<RequestAction> regex_result =
      regex_matcher_.GetAction(params, stage);
  base::TimeDelta regex_time = base::TimeTicks::Now() - start_time;
  std::optional<RequestAction> url_pattern_result =
      url_matcher_.GetAction(params, stage);
  std::optional<RequestAction> final_result = GetMaxPriorityAction(
      std::move(url_pattern_result), std::move(regex_result));
  base::TimeDelta total_time = base::TimeTicks::Now() - start_time;
  int regex_rules_count = GetRegexRulesCount(stage);
  int rules_count = GetRulesCount(stage);

  switch (stage) {
    case RulesetMatchingStage::kOnBeforeRequest:
      RecordOnBeforeRequestActionTime(start_time, regex_time, total_time,
                                      rules_count, regex_rules_count);
      break;
    case RulesetMatchingStage::kOnHeadersReceived:
      RecordOnHeadersReceivedActionTime(start_time, regex_time, total_time,
                                        rules_count, regex_rules_count);
      break;
  }

  return final_result;
}

std::vector<RequestAction> RulesetMatcher::GetModifyHeadersActions(
    const RequestParams& params,
    RulesetMatchingStage stage,
    std::optional<uint64_t> min_priority) const {
  std::vector<RequestAction> modify_header_actions =
      url_matcher_.GetModifyHeadersActions(params, stage, min_priority);

  std::vector<RequestAction> regex_modify_header_actions =
      regex_matcher_.GetModifyHeadersActions(params, stage, min_priority);

  modify_header_actions.insert(
      modify_header_actions.end(),
      std::make_move_iterator(regex_modify_header_actions.begin()),
      std::make_move_iterator(regex_modify_header_actions.end()));

  return modify_header_actions;
}

bool RulesetMatcher::IsExtraHeadersMatcher() const {
  return url_matcher_.IsExtraHeadersMatcher() ||
         regex_matcher_.IsExtraHeadersMatcher();
}

size_t RulesetMatcher::GetRulesCount() const {
  return url_matcher_.GetRulesCount() + regex_matcher_.GetRulesCount();
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
  url_matcher_.OnRenderFrameCreated(host);
  regex_matcher_.OnRenderFrameCreated(host);
}

void RulesetMatcher::OnRenderFrameDeleted(content::RenderFrameHost* host) {
  url_matcher_.OnRenderFrameDeleted(host);
  regex_matcher_.OnRenderFrameDeleted(host);
}

void RulesetMatcher::OnDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  url_matcher_.OnDidFinishNavigation(navigation_handle);
  regex_matcher_.OnDidFinishNavigation(navigation_handle);
}

std::optional<RequestAction>
RulesetMatcher::GetAllowlistedFrameActionForTesting(
    content::RenderFrameHost* host) const {
  return GetMaxPriorityAction(
      url_matcher_.GetAllowlistedFrameActionForTesting(host),     // IN-TEST
      regex_matcher_.GetAllowlistedFrameActionForTesting(host));  // IN-TEST
}

void RulesetMatcher::SetDisabledRuleIds(base::flat_set<int> disabled_rule_ids) {
  url_matcher_.SetDisabledRuleIds(std::move(disabled_rule_ids));
}

const base::flat_set<int>& RulesetMatcher::GetDisabledRuleIdsForTesting()
    const {
  return url_matcher_.GetDisabledRuleIdsForTesting();  // IN-TEST
}

size_t RulesetMatcher::GetRulesCount(RulesetMatchingStage stage) const {
  switch (stage) {
    case RulesetMatchingStage::kOnBeforeRequest:
      return url_matcher_.GetBeforeRequestRulesCount() +
             regex_matcher_.GetBeforeRequestRulesCount();
    case RulesetMatchingStage::kOnHeadersReceived:
      return url_matcher_.GetHeadersReceivedRulesCount() +
             regex_matcher_.GetHeadersReceivedRulesCount();
  }

  NOTREACHED_IN_MIGRATION();
  return 0u;
}

size_t RulesetMatcher::GetRegexRulesCount(RulesetMatchingStage stage) const {
  switch (stage) {
    case RulesetMatchingStage::kOnBeforeRequest:
      return regex_matcher_.GetBeforeRequestRulesCount();
    case RulesetMatchingStage::kOnHeadersReceived:
      return regex_matcher_.GetHeadersReceivedRulesCount();
  }

  NOTREACHED_IN_MIGRATION();
  return 0u;
}

}  // namespace extensions::declarative_net_request

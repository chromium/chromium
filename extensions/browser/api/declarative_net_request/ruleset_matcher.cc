// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {

// static
LoadRulesetResult RulesetMatcher::CreateVerifiedMatcher(
    const FileBackedRulesetSource& source,
    int expected_ruleset_checksum,
    std::unique_ptr<RulesetMatcher>* matcher) {
  DCHECK(matcher);
  DCHECK(IsAPIAvailable());

  base::ElapsedTimer timer;

  if (!base::PathExists(source.indexed_path()))
    return LoadRulesetResult::kErrorInvalidPath;

  std::string ruleset_data;
  if (!base::ReadFileToString(source.indexed_path(), &ruleset_data))
    return LoadRulesetResult::kErrorCannotReadFile;

  if (!StripVersionHeaderAndParseVersion(&ruleset_data))
    return LoadRulesetResult::kErrorVersionMismatch;

  // This guarantees that no memory access will end up outside the buffer.
  if (!IsValidRulesetData(
          base::make_span(reinterpret_cast<const uint8_t*>(ruleset_data.data()),
                          ruleset_data.size()),
          expected_ruleset_checksum)) {
    return LoadRulesetResult::kErrorChecksumMismatch;
  }

  UMA_HISTOGRAM_TIMES(
      "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
      timer.Elapsed());

  // Using WrapUnique instead of make_unique since this class has a private
  // constructor.
  *matcher = base::WrapUnique(new RulesetMatcher(
      std::move(ruleset_data), source.id(), source.extension_id()));
  return LoadRulesetResult::kSuccess;
}

RulesetMatcher::~RulesetMatcher() = default;

base::Optional<RequestAction> RulesetMatcher::GetBeforeRequestAction(
    const RequestParams& params) const {
  return GetMaxPriorityAction(
      url_pattern_index_matcher_.GetBeforeRequestAction(params),
      regex_matcher_.GetBeforeRequestAction(params));
}

std::vector<RequestAction> RulesetMatcher::GetModifyHeadersActions(
    const RequestParams& params,
    base::Optional<uint64_t> min_priority) const {
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

void RulesetMatcher::OnRenderFrameCreated(content::RenderFrameHost* host) {
  url_pattern_index_matcher_.OnRenderFrameCreated(host);
  regex_matcher_.OnRenderFrameCreated(host);
}

void RulesetMatcher::OnRenderFrameDeleted(content::RenderFrameHost* host) {
  url_pattern_index_matcher_.OnRenderFrameDeleted(host);
  regex_matcher_.OnRenderFrameDeleted(host);
}

void RulesetMatcher::OnDidFinishNavigation(content::RenderFrameHost* host) {
  url_pattern_index_matcher_.OnDidFinishNavigation(host);
  regex_matcher_.OnDidFinishNavigation(host);
}

base::Optional<RequestAction>
RulesetMatcher::GetAllowlistedFrameActionForTesting(
    content::RenderFrameHost* host) const {
  return GetMaxPriorityAction(
      url_pattern_index_matcher_.GetAllowlistedFrameActionForTesting(host),
      regex_matcher_.GetAllowlistedFrameActionForTesting(host));
}

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

}  // namespace declarative_net_request
}  // namespace extensions

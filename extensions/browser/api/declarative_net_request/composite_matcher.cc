// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <functional>
#include <iterator>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
using PageAccess = PermissionsData::PageAccess;
using ActionInfo = CompositeMatcher::ActionInfo;

namespace {

bool AreIDsUnique(const CompositeMatcher::MatcherList& matchers) {
  std::set<RulesetID> ids;
  for (const auto& matcher : matchers) {
    bool did_insert = ids.insert(matcher->id()).second;
    if (!did_insert)
      return false;
  }

  return true;
}

// Helper to log the time taken in CompositeMatcher::GetBeforeRequestAction.
class ScopedGetBeforeRequestActionTimer {
 public:
  ScopedGetBeforeRequestActionTimer() = default;
  ~ScopedGetBeforeRequestActionTimer() {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest.EvaluateBeforeRequestTime."
        "SingleExtension2",
        timer_.Elapsed(), base::Microseconds(1), base::Milliseconds(50), 50);
  }

 private:
  base::ElapsedTimer timer_;
};

}  // namespace

ActionInfo::ActionInfo() = default;
ActionInfo::ActionInfo(absl::optional<RequestAction> action,
                       bool notify_request_withheld)
    : action(std::move(action)),
      notify_request_withheld(notify_request_withheld) {}

ActionInfo::~ActionInfo() = default;

ActionInfo::ActionInfo(ActionInfo&&) = default;
ActionInfo& ActionInfo::operator=(ActionInfo&& other) = default;

CompositeMatcher::CompositeMatcher(MatcherList matchers,
                                   HostPermissionsAlwaysRequired mode)
    : matchers_(std::move(matchers)), host_permissions_always_required_(mode) {
  DCHECK(AreIDsUnique(matchers_));
}

CompositeMatcher::~CompositeMatcher() = default;

const RulesetMatcher* CompositeMatcher::GetMatcherWithID(RulesetID id) const {
  auto it = base::ranges::find(matchers_, id, &RulesetMatcher::id);
  return it == matchers_.end() ? nullptr : it->get();
}

void CompositeMatcher::AddOrUpdateRuleset(
    std::unique_ptr<RulesetMatcher> matcher) {
  MatcherList matchers;
  matchers.push_back(std::move(matcher));
  AddOrUpdateRulesets(std::move(matchers));
}

void CompositeMatcher::AddOrUpdateRulesets(MatcherList matchers) {
  std::set<RulesetID> ids_to_remove;
  for (const auto& matcher : matchers)
    ids_to_remove.insert(matcher->id());

  RemoveRulesetsWithIDs(ids_to_remove);
  matchers_.insert(matchers_.end(), std::make_move_iterator(matchers.begin()),
                   std::make_move_iterator(matchers.end()));
  OnMatchersModified();
}

void CompositeMatcher::RemoveRulesetsWithIDs(const std::set<RulesetID>& ids) {
  size_t erased_count = base::EraseIf(
      matchers_, [&ids](const std::unique_ptr<RulesetMatcher>& matcher) {
        return base::Contains(ids, matcher->id());
      });

  if (erased_count > 0)
    OnMatchersModified();
}

std::set<RulesetID> CompositeMatcher::ComputeStaticRulesetIDs() const {
  std::set<RulesetID> result;
  for (const std::unique_ptr<RulesetMatcher>& matcher : matchers_) {
    if (matcher->id() == kDynamicRulesetID)
      continue;

    result.insert(matcher->id());
  }

  return result;
}

ActionInfo CompositeMatcher::GetBeforeRequestAction(
    const RequestParams& params,
    PageAccess page_access) const {
  ScopedGetBeforeRequestActionTimer timer;

  bool always_require_host_permissions =
      host_permissions_always_required_ == HostPermissionsAlwaysRequired::kTrue;
  if (always_require_host_permissions) {
    // We shouldn't be evaluating this ruleset if host permissions are always
    // required but this extension doesn't have access to the request.
    DCHECK(page_access == PermissionsData::PageAccess::kAllowed ||
           page_access == PermissionsData::PageAccess::kWithheld);
  }

  absl::optional<RequestAction> final_action;

  // The priority of the highest priority matching allow or allowAllRequests
  // rule within this matcher, or absl::nullopt otherwise.
  absl::optional<uint64_t> max_allow_rule_priority;

  for (const auto& matcher : matchers_) {
    absl::optional<RequestAction> action =
        matcher->GetBeforeRequestAction(params);
    if (!action)
      continue;

    if (action->IsAllowOrAllowAllRequests()) {
      max_allow_rule_priority =
          std::max(max_allow_rule_priority.value_or(0), action->index_priority);
    }

    final_action =
        GetMaxPriorityAction(std::move(final_action), std::move(action));
  }

  params.allow_rule_max_priority[this] = max_allow_rule_priority;

  if (!final_action)
    return ActionInfo();

  bool requires_host_permission =
      always_require_host_permissions ||
      final_action->type == RequestAction::Type::REDIRECT;
  if (!requires_host_permission || page_access == PageAccess::kAllowed) {
    return ActionInfo(std::move(final_action),
                      false /* notify_request_withheld */);
  }

  // `requires_host_permission` is true and `page_access` is withheld or denied.
  bool notify_request_withheld = page_access == PageAccess::kWithheld &&
                                 !final_action->IsAllowOrAllowAllRequests();
  return ActionInfo(absl::nullopt, notify_request_withheld);
}

std::vector<RequestAction> CompositeMatcher::GetModifyHeadersActions(
    const RequestParams& params) const {
  std::vector<RequestAction> modify_headers_actions;
  DCHECK(params.allow_rule_max_priority.contains(this));

  // The priority of the highest priority matching allow or allowAllRequests
  // rule within this matcher, or absl::nullopt if no such rule exists.
  absl::optional<uint64_t> max_allow_rule_priority =
      params.allow_rule_max_priority[this];

  for (const auto& matcher : matchers_) {
    // Plumb |max_allow_rule_priority| into GetModifyHeadersActions so that
    // modifyHeaders rules with priorities less than or equal to the highest
    // priority matching allow/allowAllRequests rule are ignored.
    std::vector<RequestAction> actions_for_matcher =
        matcher->GetModifyHeadersActions(params, max_allow_rule_priority);

    modify_headers_actions.insert(
        modify_headers_actions.end(),
        std::make_move_iterator(actions_for_matcher.begin()),
        std::make_move_iterator(actions_for_matcher.end()));
  }

  // Sort |modify_headers_actions| in descending order of priority.
  std::sort(modify_headers_actions.begin(), modify_headers_actions.end(),
            std::greater<>());
  return modify_headers_actions;
}

bool CompositeMatcher::HasAnyExtraHeadersMatcher() const {
  if (!has_any_extra_headers_matcher_.has_value())
    has_any_extra_headers_matcher_ = ComputeHasAnyExtraHeadersMatcher();
  return has_any_extra_headers_matcher_.value();
}

void CompositeMatcher::OnRenderFrameCreated(content::RenderFrameHost* host) {
  for (auto& matcher : matchers_)
    matcher->OnRenderFrameCreated(host);
}

void CompositeMatcher::OnRenderFrameDeleted(content::RenderFrameHost* host) {
  for (auto& matcher : matchers_)
    matcher->OnRenderFrameDeleted(host);
}

void CompositeMatcher::OnDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  for (auto& matcher : matchers_)
    matcher->OnDidFinishNavigation(navigation_handle);
}

void CompositeMatcher::OnMatchersModified() {
  DCHECK(AreIDsUnique(matchers_));

  // Clear the renderers' cache so that they take the updated rules into
  // account.
  ClearRendererCacheOnNavigation();

  has_any_extra_headers_matcher_.reset();
}

bool CompositeMatcher::ComputeHasAnyExtraHeadersMatcher() const {
  for (const auto& matcher : matchers_) {
    if (matcher->IsExtraHeadersMatcher())
      return true;
  }
  return false;
}

}  // namespace declarative_net_request
}  // namespace extensions

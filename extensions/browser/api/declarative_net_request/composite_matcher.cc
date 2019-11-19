// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
using PageAccess = PermissionsData::PageAccess;
using RedirectActionInfo = CompositeMatcher::RedirectActionInfo;

namespace {

bool AreIDsUnique(const CompositeMatcher::MatcherList& matchers) {
  std::set<size_t> ids;
  for (const auto& matcher : matchers) {
    bool did_insert = ids.insert(matcher->id()).second;
    if (!did_insert)
      return false;
  }

  return true;
}

bool AreSortedPrioritiesUnique(const CompositeMatcher::MatcherList& matchers) {
  base::Optional<size_t> previous_priority;
  for (const auto& matcher : matchers) {
    if (matcher->priority() == previous_priority)
      return false;
    previous_priority = matcher->priority();
  }

  return true;
}

}  // namespace

RedirectActionInfo::RedirectActionInfo(base::Optional<RequestAction> action,
                                       bool notify_request_withheld)
    : action(std::move(action)),
      notify_request_withheld(notify_request_withheld) {
  if (action)
    DCHECK_EQ(RequestAction::Type::REDIRECT, action->type);
}

RedirectActionInfo::~RedirectActionInfo() = default;

RedirectActionInfo::RedirectActionInfo(RedirectActionInfo&&) = default;
RedirectActionInfo& RedirectActionInfo::operator=(RedirectActionInfo&& other) =
    default;

CompositeMatcher::CompositeMatcher(MatcherList matchers,
                                   ActionTracker* action_tracker)
    : matchers_(std::move(matchers)), action_tracker_(action_tracker) {
  SortMatchersByPriority();
  DCHECK(AreIDsUnique(matchers_));
}

CompositeMatcher::~CompositeMatcher() = default;

void CompositeMatcher::AddOrUpdateRuleset(
    std::unique_ptr<RulesetMatcher> new_matcher) {
  // A linear search is ok since the number of rulesets per extension is
  // expected to be quite small.
  auto it = std::find_if(
      matchers_.begin(), matchers_.end(),
      [&new_matcher](const std::unique_ptr<RulesetMatcher>& matcher) {
        return new_matcher->id() == matcher->id();
      });

  if (it == matchers_.end()) {
    matchers_.push_back(std::move(new_matcher));
    SortMatchersByPriority();
  } else {
    // Update the matcher. The priority for a given ID should remain the same.
    DCHECK_EQ(new_matcher->priority(), (*it)->priority());
    *it = std::move(new_matcher);
  }

  // Clear the renderers' cache so that they take the updated rules into
  // account.
  ClearRendererCacheOnNavigation();
  has_any_extra_headers_matcher_.reset();
}

base::Optional<RequestAction> CompositeMatcher::GetBlockOrCollapseAction(
    const RequestParams& params) const {
  // TODO(karandeepb): change this to report time in micro-seconds.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldBlockRequestTime."
      "SingleExtension");

  for (const auto& matcher : matchers_) {
    if (HasAllowAction(*matcher, params))
      return base::nullopt;

    base::Optional<RequestAction> action =
        matcher->GetBlockOrCollapseAction(params);
    if (action)
      return action;
  }

  return base::nullopt;
}

RedirectActionInfo CompositeMatcher::GetRedirectAction(
    const RequestParams& params,
    PageAccess page_access) const {
  // TODO(karandeepb): change this to report time in micro-seconds.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldRedirectRequestTime."
      "SingleExtension");

  bool notify_request_withheld = false;
  for (const auto& matcher : matchers_) {
    if (HasAllowAction(*matcher, params)) {
      return RedirectActionInfo(base::nullopt /* action */,
                                false /* notify_request_withheld */);
    }

    if (page_access == PageAccess::kAllowed) {
      base::Optional<RequestAction> action =
          matcher->GetRedirectOrUpgradeActionByPriority(params);
      if (!action)
        continue;

      return RedirectActionInfo(std::move(action),
                                false /* notify_request_withheld */);
    }

    // If the extension has no host permissions for the request, it can still
    // upgrade the request.
    base::Optional<RequestAction> upgrade_action =
        matcher->GetUpgradeAction(params);
    if (upgrade_action) {
      return RedirectActionInfo(std::move(upgrade_action),
                                false /* notify_request_withheld */);
    }

    notify_request_withheld |= (page_access == PageAccess::kWithheld &&
                                matcher->GetRedirectAction(params));
  }

  return RedirectActionInfo(base::nullopt /* action */,
                            notify_request_withheld);
}

uint8_t CompositeMatcher::GetRemoveHeadersMask(
    const RequestParams& params,
    uint8_t ignored_mask,
    std::vector<RequestAction>* remove_headers_actions) const {
  uint8_t mask = 0;
  for (const auto& matcher : matchers_) {
    // The allow rule will override lower priority remove header rules.
    if (HasAllowAction(*matcher, params))
      return mask;
    mask |= matcher->GetRemoveHeadersMask(params, mask | ignored_mask,
                                          remove_headers_actions);
  }

  DCHECK(!(mask & ignored_mask));
  return mask;
}

bool CompositeMatcher::HasAnyExtraHeadersMatcher() const {
  if (!has_any_extra_headers_matcher_.has_value())
    has_any_extra_headers_matcher_ = ComputeHasAnyExtraHeadersMatcher();
  return has_any_extra_headers_matcher_.value();
}

bool CompositeMatcher::ComputeHasAnyExtraHeadersMatcher() const {
  for (const auto& matcher : matchers_) {
    if (matcher->IsExtraHeadersMatcher())
      return true;
  }
  return false;
}

void CompositeMatcher::SortMatchersByPriority() {
  std::sort(matchers_.begin(), matchers_.end(),
            [](const std::unique_ptr<RulesetMatcher>& a,
               const std::unique_ptr<RulesetMatcher>& b) {
              return a->priority() > b->priority();
            });
  DCHECK(AreSortedPrioritiesUnique(matchers_));
}

bool CompositeMatcher::HasAllowAction(const RulesetMatcher& matcher,
                                      const RequestParams& params) const {
  if (!params.allow_rule_cache.contains(&matcher)) {
    base::Optional<RequestAction> allow_action = matcher.GetAllowAction(params);
    params.allow_rule_cache[&matcher] = allow_action.has_value();

    // OnRuleMatched is called only once, when the allow action is entered into
    // the cache. This is done because an allow rule might override an action
    // multiple times during a request and extraneous matches should be ignored.
    if (allow_action && action_tracker_ && params.request_info)
      action_tracker_->OnRuleMatched(*allow_action, *params.request_info);
  }

  return params.allow_rule_cache[&matcher];
}

}  // namespace declarative_net_request
}  // namespace extensions

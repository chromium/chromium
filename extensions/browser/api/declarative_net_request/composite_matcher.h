// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace declarative_net_request {

class ActionTracker;
struct RequestAction;

// Per extension instance which manages the different rulesets for an extension
// while respecting their priorities.
class CompositeMatcher {
 public:
  struct RedirectActionInfo {
    RedirectActionInfo(base::Optional<RequestAction> action, bool notify);
    ~RedirectActionInfo();
    RedirectActionInfo(RedirectActionInfo&& other);
    RedirectActionInfo& operator=(RedirectActionInfo&& other);

    // The action to be taken for this request. If specified, the action type
    // must be |REDIRECT|.
    base::Optional<RequestAction> action;

    // Whether the extension should be notified that the request was unable to
    // be redirected as the extension lacks the appropriate host permission for
    // the request.
    bool notify_request_withheld = false;

    DISALLOW_COPY_AND_ASSIGN(RedirectActionInfo);
  };

  using MatcherList = std::vector<std::unique_ptr<RulesetMatcher>>;

  // Each RulesetMatcher should have a distinct ID and priority.
  explicit CompositeMatcher(MatcherList matchers,
                            ActionTracker* action_tracker);
  ~CompositeMatcher();

  // Adds the |new_matcher| to the list of matchers. If a matcher with the
  // corresponding ID is already present, updates the matcher.
  void AddOrUpdateRuleset(std::unique_ptr<RulesetMatcher> new_matcher);

  // Returns a RequestAction if the network request specified by |params| should
  // be blocked.
  base::Optional<RequestAction> GetBlockOrCollapseAction(
      const RequestParams& params) const;

  // Returns a RedirectActionInfo struct containing a RequestAction if the
  // request is to be redirected, and whether the extension should be notified
  // if its access to the request is withheld.
  RedirectActionInfo GetRedirectAction(
      const RequestParams& params,
      PermissionsData::PageAccess page_access) const;

  // Returns the bitmask of headers to remove from the request corresponding to
  // rules matched from this extension. The bitmask corresponds to
  // RemoveHeadersMask type. |ignored_mask| denotes the current mask of headers
  // to be skipped for evaluation and is excluded in the return value.
  uint8_t GetRemoveHeadersMask(
      const RequestParams& params,
      uint8_t ignored_mask,
      std::vector<RequestAction>* remove_headers_actions) const;

  // Returns whether this modifies "extraHeaders".
  bool HasAnyExtraHeadersMatcher() const;

 private:
  bool ComputeHasAnyExtraHeadersMatcher() const;

  // Sorts |matchers_| in descending order of priority.
  void SortMatchersByPriority();

  // Check if |matcher| has an allow action for |params| and tracks the action
  // if needed.
  bool HasAllowAction(const RulesetMatcher& matcher,
                      const RequestParams& params) const;

  // Sorted by priority in descending order.
  MatcherList matchers_;

  // Denotes the cached return value for |HasAnyExtraHeadersMatcher|. Care must
  // be taken to reset this as this object is modified.
  mutable base::Optional<bool> has_any_extra_headers_matcher_;

  // Used to track when allow rules are matched; can be null during unit tests.
  // Owned by RulesMonitorService.
  ActionTracker* action_tracker_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/common/permissions/permissions_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace extensions::declarative_net_request {

struct RequestAction;

// Per extension instance which manages the different rulesets for an extension.
class CompositeMatcher {
 public:
  struct ActionInfo {
    // Constructs a no-op ActionInfo object.
    ActionInfo();

    ActionInfo(std::optional<RequestAction> action,
               bool notify_request_withheld);

    ActionInfo(const ActionInfo&) = delete;
    ActionInfo& operator=(const ActionInfo&) = delete;

    ~ActionInfo();
    ActionInfo(ActionInfo&& other);
    ActionInfo& operator=(ActionInfo&& other);

    // The action to be taken for this request.
    std::optional<RequestAction> action;

    // Whether the extension should be notified that the request was unable to
    // be redirected as the extension lacks the appropriate host permission for
    // the request. Can only be true for redirect actions.
    bool notify_request_withheld = false;
  };

  using MatcherList = std::vector<std::unique_ptr<RulesetMatcher>>;

  // Each RulesetMatcher should have a distinct RulesetID.
  CompositeMatcher(MatcherList matchers,
                   const ExtensionId& extension_id,
                   HostPermissionsAlwaysRequired mode);

  CompositeMatcher(const CompositeMatcher&) = delete;
  CompositeMatcher& operator=(const CompositeMatcher&) = delete;

  ~CompositeMatcher();

  const MatcherList& matchers() const { return matchers_; }

  HostPermissionsAlwaysRequired host_permissions_always_required() const {
    return host_permissions_always_required_;
  }

  // Returns a pointer to RulesetMatcher with the given |id| if one is present.
  const RulesetMatcher* GetMatcherWithID(RulesetID id) const;

  // Inserts |matcher|, overwriting any existing RulesetMatcher with the same
  // RulesetID.
  void AddOrUpdateRuleset(std::unique_ptr<RulesetMatcher> matcher);

  // Inserts |matchers| overwriting any matchers with the same RulesetID.
  void AddOrUpdateRulesets(CompositeMatcher::MatcherList matchers);

  // Erases RulesetMatchers with the given RulesetIDs.
  void RemoveRulesetsWithIDs(const std::set<RulesetID>& ids);

  // Computes and returns the set of static RulesetIDs corresponding to
  // |matchers_|.
  std::set<RulesetID> ComputeStaticRulesetIDs() const;

  // Returns a RequestAction for the network request specified by |params|, or
  // std::nullopt if there is no matching rule.
  ActionInfo GetAction(const RequestParams& params,
                       RulesetMatchingStage stage,
                       PermissionsData::PageAccess page_access) const;

  // Returns all matching RequestActions for the request corresponding to
  // modifyHeaders rules matched from this extension, sorted in descending order
  // by rule priority.
  std::vector<RequestAction> GetModifyHeadersActions(
      const RequestParams& params,
      RulesetMatchingStage stage) const;

  // Returns whether this modifies "extraHeaders".
  bool HasAnyExtraHeadersMatcher() const;

  void OnRenderFrameCreated(content::RenderFrameHost* host);
  void OnRenderFrameDeleted(content::RenderFrameHost* host);
  void OnDidFinishNavigation(content::NavigationHandle* navigation_handle);

  // Returns if this extension has any rulesets with rules that are matched in
  // the corresponding matching `stage`.
  bool HasRulesets(RulesetMatchingStage stage) const;

 private:
  // This must be called whenever |matchers_| are modified.
  void OnMatchersModified();

  bool ComputeHasAnyExtraHeadersMatcher() const;

  // The RulesetMatchers, in an arbitrary order.
  MatcherList matchers_;

  // Denotes the cached return value for |HasAnyExtraHeadersMatcher|. Care must
  // be taken to reset this as this object is modified.
  mutable std::optional<bool> has_any_extra_headers_matcher_;

  // The id of the extension associated with this matcher.
  const ExtensionId extension_id_;

  const HostPermissionsAlwaysRequired host_permissions_always_required_;
};

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_

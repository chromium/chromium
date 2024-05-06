// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/permissions_data.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class RenderFrameHost;
}

namespace extensions {
class ExtensionPrefs;
class PermissionHelper;
struct WebRequestInfo;

namespace declarative_net_request {
class CompositeMatcher;
struct RequestAction;
struct RequestParams;

// Manages the set of active rulesets for the Declarative Net Request API. Can
// be constructed on any sequence but must be accessed and destroyed from the
// same sequence.
class RulesetManager {
 public:
  explicit RulesetManager(content::BrowserContext* browser_context);

  RulesetManager(const RulesetManager&) = delete;
  RulesetManager& operator=(const RulesetManager&) = delete;

  ~RulesetManager();

  // An observer used for testing purposes.
  class TestObserver {
   public:
    virtual void OnEvaluateRequest(const WebRequestInfo& request,
                                   bool is_incognito_context) {}

    // Called whenever a ruleset is added or removed.
    virtual void OnRulesetCountChanged(size_t new_count) {}

   protected:
    virtual ~TestObserver() {}
  };

  // Adds the ruleset for the given |extension_id|. Should not be called twice
  // in succession for an extension.
  void AddRuleset(const ExtensionId& extension_id,
                  std::unique_ptr<CompositeMatcher> matcher);

  // Removes the ruleset for |extension_id|. Should be called only after a
  // corresponding AddRuleset.
  void RemoveRuleset(const ExtensionId& extension_id);

  // Returns the set of extensions which have active rulesets.
  std::set<ExtensionId> GetExtensionsWithRulesets() const;

  // Returns the CompositeMatcher corresponding to the |extension_id| or null
  // if no matcher is present for the extension.
  CompositeMatcher* GetMatcherForExtension(const ExtensionId& extension_id);
  const CompositeMatcher* GetMatcherForExtension(
      const ExtensionId& extension_id) const;

  // Returns the action to take for the given request before it is sent.
  // Note: this can return an `ALLOW` or `ALLOW_ALL_REQUESTS` rule which is
  // effectively a no-op. We do this to ensure that matched allow rules are
  // correctly tracked by the `getMatchedRules` and `OnRuleMatchedDebug` APIs.
  // Note: the returned action is owned by `request`.
  const std::vector<RequestAction>& EvaluateBeforeRequest(
      const WebRequestInfo& request,
      bool is_incognito_context) const;

  // Returns the action to take for the given request after response headers
  // have been received.
  // Note: See comments for `EvaluateBeforeRequest` above for notes on returning
  // an ALLOW or ALLOW_ALL_REQUESTS action.
  std::vector<RequestAction> EvaluateRequestWithHeaders(
      const WebRequestInfo& request,
      const net::HttpResponseHeaders* response_headers,
      bool is_incognito_context) const;

  // Returns true if there is an active matcher which modifies "extraHeaders".
  bool HasAnyExtraHeadersMatcher() const;

  // Returns true if there is a matcher which modifies "extraHeaders" for the
  // given |request|.
  bool HasExtraHeadersMatcherForRequest(const WebRequestInfo& request,
                                        bool is_incognito_context) const;

  void OnRenderFrameCreated(content::RenderFrameHost* host);
  void OnRenderFrameDeleted(content::RenderFrameHost* host);
  void OnDidFinishNavigation(content::NavigationHandle* navigation_handle);

  // Returns if there are any matchers containing rules for the corresponding
  // request matching `stage`.
  bool HasRulesets(RulesetMatchingStage stage) const;

  // Merges two lists of modifyHeaders actions and returns a list containing
  // actions from both lists sorted in descending order of priority.
  std::vector<RequestAction> MergeModifyHeaderActions(
      std::vector<RequestAction> lhs_actions,
      std::vector<RequestAction> rhs_actions) const;

  // Returns the number of CompositeMatchers currently being managed.
  size_t GetMatcherCountForTest() const { return rulesets_.size(); }

  // Sets the TestObserver. Client maintains ownership of |observer|.
  void SetObserverForTest(TestObserver* observer);

 private:
  struct ExtensionRulesetData {
    ExtensionRulesetData(const ExtensionId& extension_id,
                         const base::Time& extension_install_time,
                         std::unique_ptr<CompositeMatcher> matcher);
    ExtensionRulesetData(const ExtensionRulesetData&) = delete;
    ExtensionRulesetData(ExtensionRulesetData&& other);

    ExtensionRulesetData& operator=(const ExtensionRulesetData&) = delete;
    ExtensionRulesetData& operator=(ExtensionRulesetData&& other);

    ~ExtensionRulesetData();

    ExtensionId extension_id;
    base::Time extension_install_time;
    std::unique_ptr<CompositeMatcher> matcher;

    bool operator<(const ExtensionRulesetData& other) const;
  };

  using RulesetAndPageAccess =
      std::pair<const ExtensionRulesetData*, PermissionsData::PageAccess>;

  std::optional<RequestAction> GetAction(
      const std::vector<RulesetAndPageAccess>& rulesets,
      const WebRequestInfo& request,
      const RequestParams& params,
      RulesetMatchingStage stage) const;

  // Returns the list of matching modifyHeaders actions sorted in descending
  // order of priority (|rulesets| is sorted in descending order of extension
  // priority.)
  std::vector<RequestAction> GetModifyHeadersActions(
      const std::vector<RulesetAndPageAccess>& rulesets,
      const WebRequestInfo& request,
      const RequestParams& params,
      RulesetMatchingStage stage) const;

  // Helper for EvaluateRequest.
  std::vector<RequestAction> EvaluateRequestInternal(
      const WebRequestInfo& request,
      const net::HttpResponseHeaders* response_headers,
      bool is_incognito_context) const;

  // Returns true if the given |request| should be evaluated for
  // blocking/redirection.
  bool ShouldEvaluateRequest(const WebRequestInfo& request) const;

  // Returns whether `ruleset` should be evaluated for the given `request`.
  // Returns true if it should and populates `host_permission_access`.
  bool ShouldEvaluateRulesetForRequest(
      const ExtensionRulesetData& ruleset,
      const WebRequestInfo& request,
      bool is_incognito_context,
      PermissionsData::PageAccess& host_permission_access) const;

  // Sorted in decreasing order of |extension_install_time|.
  // Use a flat_set instead of std::set/map. This makes [Add/Remove]Ruleset
  // O(n), but it's fine since the no. of rulesets are expected to be quite
  // small.
  base::flat_set<ExtensionRulesetData> rulesets_;
  // Maps an extension ID to its install time. Used to determine an extension's
  // ruleset matching priority order relative to other extensions, with matched
  // rules/actions from more recently installed extensions having higher
  // precedence.
  std::map<ExtensionId, base::Time> extension_install_times_;

  // Non-owning pointer to BrowserContext.
  const raw_ptr<content::BrowserContext> browser_context_;

  // Guaranteed to be valid through-out the lifetime of this instance.
  const raw_ptr<ExtensionPrefs> prefs_;
  const raw_ptr<PermissionHelper> permission_helper_;

  // Non-owning pointer to TestObserver.
  raw_ptr<TestObserver> test_observer_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_

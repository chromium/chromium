// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_

#include <stddef.h>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"

class GURL;

namespace extensions {
class InfoMap;
struct WebRequestInfo;

namespace declarative_net_request {
class RulesetMatcher;

// Manages the set of active rulesets for the Declarative Net Request API. Can
// be constructed on any sequence but must be accessed and destroyed from the
// same sequence.
class RulesetManager {
 public:
  enum class Action {
    NONE,
    // Block the network request.
    BLOCK,
    // Block the network request and collapse the corresponding DOM element.
    COLLAPSE,
    // Redirect the network request.
    REDIRECT,
  };

  explicit RulesetManager(const InfoMap* info_map);
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
                  std::unique_ptr<RulesetMatcher> ruleset_matcher,
                  URLPatternSet allowed_pages);

  // Removes the ruleset for |extension_id|. Should be called only after a
  // corresponding AddRuleset.
  void RemoveRuleset(const ExtensionId& extension_id);

  void UpdateAllowedPages(const ExtensionId& extension_id,
                          URLPatternSet allowed_pages);

  // Returns the action to take for the given request. |redirect_url| will be
  // populated if the returned action is |REDIRECT|. Blocking rules have higher
  // priority than redirect rules. For determining the |redirect_url|, most
  // recently installed extensions are given preference. |redirect_url| must not
  // be null.
  Action EvaluateRequest(const WebRequestInfo& request,
                         bool is_incognito_context,
                         GURL* redirect_url) const;

  // Returns the number of RulesetMatcher currently being managed.
  size_t GetMatcherCountForTest() const { return rulesets_.size(); }

  // Sets the TestObserver. Client maintains ownership of |observer|.
  void SetObserverForTest(TestObserver* observer);

 private:
  struct ExtensionRulesetData {
    ExtensionRulesetData(const ExtensionId& extension_id,
                         const base::Time& extension_install_time,
                         std::unique_ptr<RulesetMatcher> matcher,
                         URLPatternSet allowed_pages);
    ~ExtensionRulesetData();
    ExtensionRulesetData(ExtensionRulesetData&& other);
    ExtensionRulesetData& operator=(ExtensionRulesetData&& other);

    ExtensionId extension_id;
    base::Time extension_install_time;
    std::unique_ptr<RulesetMatcher> matcher;
    URLPatternSet allowed_pages;

    bool operator<(const ExtensionRulesetData& other) const;

    DISALLOW_COPY_AND_ASSIGN(ExtensionRulesetData);
  };

  // Returns true if the given |request| should be evaluated for
  // blocking/redirection.
  bool ShouldEvaluateRequest(const WebRequestInfo& request) const;

  // Returns whether |ruleset| should be evaluated for the given |request|.
  // Note: this does not take the extension's host permissions into account.
  bool ShouldEvaluateRulesetForRequest(const ExtensionRulesetData& ruleset,
                                       const WebRequestInfo& request,
                                       bool is_incognito_context) const;

  // Sorted in decreasing order of |extension_install_time|.
  // Use a flat_set instead of std::set/map. This makes [Add/Remove]Ruleset
  // O(n), but it's fine since the no. of rulesets are expected to be quite
  // small.
  base::flat_set<ExtensionRulesetData> rulesets_;

  // Non-owning pointer to InfoMap. Owns us.
  const InfoMap* const info_map_;

  // Non-owning pointer to TestObserver.
  TestObserver* test_observer_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(RulesetManager);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_

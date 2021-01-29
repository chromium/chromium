// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/url_matcher/url_matcher.h"
#include "extensions/browser/api/declarative/declarative_rule.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_action.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_condition.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace extensions {
class PermissionHelper;

using WebRequestRule = DeclarativeRule<WebRequestCondition, WebRequestAction>;

// The WebRequestRulesRegistry is responsible for managing
// the internal representation of rules for the Declarative Web Request API.
//
// Here is the high level overview of this functionality:
//
// api::events::Rule consists of Conditions and Actions, these are
// represented as a WebRequestRule with WebRequestConditions and
// WebRequestRuleActions.
//
// WebRequestConditions represent JSON dictionaries as the following:
// {
//   'instanceType': 'URLMatcher',
//   'host_suffix': 'example.com',
//   'path_prefix': '/query',
//   'scheme': 'http'
// }
//
// The evaluation of URL related condition attributes (host_suffix, path_prefix)
// is delegated to a URLMatcher, because this is capable of evaluating many
// of such URL related condition attributes in parallel.
//
// For this, the URLRequestCondition has a URLMatcherConditionSet, which
// represents the {'host_suffix': 'example.com', 'path_prefix': '/query'} part.
// We will then ask the URLMatcher, whether a given URL
// "http://www.example.com/query/" has any matches, and the URLMatcher
// will respond with the URLMatcherConditionSet::ID. We can map this
// to the WebRequestRule and check whether also the other conditions (in this
// example 'scheme': 'http') are fulfilled.
class WebRequestRulesRegistry : public RulesRegistry {
 public:
  // |cache_delegate| can be NULL. In that case it constructs the registry with
  // storage functionality suspended.
  WebRequestRulesRegistry(content::BrowserContext* browser_context,
                          RulesCacheDelegate* cache_delegate,
                          int rules_registry_id);

  // TODO(battre): This will become an implementation detail, because we need
  // a way to also execute the actions of the rules.
  std::set<const WebRequestRule*> GetMatches(
      const WebRequestData& request_data_without_ids) const;

  // Returns which modifications should be executed on the network request
  // according to the rules registered in this registry.
  std::list<extension_web_request_api_helpers::EventResponseDelta> CreateDeltas(
      PermissionHelper* permission_helper,
      const WebRequestData& request_data,
      bool crosses_incognito);

  // Implementation of RulesRegistry:
  std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<const api::events::Rule*>& rules) override;
  std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) override;
  std::string RemoveAllRulesImpl(const std::string& extension_id) override;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 protected:
  ~WebRequestRulesRegistry() override;

  // Virtual for testing:
  virtual base::Time GetExtensionInstallationTime(
      const std::string& extension_id) const;
  virtual void ClearCacheOnNavigation();

  const std::set<const WebRequestRule*>&
  rules_with_untriggered_conditions_for_test() const {
    return rules_with_untriggered_conditions_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebRequestRulesRegistrySimpleTest, StageChecker);
  FRIEND_TEST_ALL_PREFIXES(WebRequestRulesRegistrySimpleTest,
                           HostPermissionsChecker);

  using RuleTriggers =
      std::map<url_matcher::URLMatcherConditionSet::ID, const WebRequestRule*>;
  using RulesMap =
      std::map<WebRequestRule::RuleId, std::unique_ptr<const WebRequestRule>>;
  using URLMatches = std::set<url_matcher::URLMatcherConditionSet::ID>;
  using RuleSet = std::set<const WebRequestRule*>;

  // This bundles all consistency checkers. Returns true in case of consistency
  // and MUST set |error| otherwise.
  static bool Checker(const Extension* extension,
                      const WebRequestConditionSet* conditions,
                      const WebRequestActionSet* actions,
                      std::string* error);

  // Check that the |extension| has host permissions for all URLs if actions
  // requiring them are present.
  static bool HostPermissionsChecker(const Extension* extension,
                                     const WebRequestActionSet* actions,
                                     std::string* error);

  // Check that every action is applicable in the same request stage as at
  // least one condition.
  static bool StageChecker(const WebRequestConditionSet* conditions,
                           const WebRequestActionSet* actions,
                           std::string* error);

  // Helper for RemoveRulesImpl and RemoveAllRulesImpl. Call this before
  // deleting |rule| from one of the maps in |webrequest_rules_|. It will erase
  // the rule from |rule_triggers_| and |rules_with_untriggered_conditions_|,
  // and add every of the rule's URLMatcherConditionSet to
  // |remove_from_url_matcher|, so that the caller can remove them from the
  // matcher later.
  void CleanUpAfterRule(const WebRequestRule* rule,
                        std::vector<url_matcher::URLMatcherConditionSet::ID>*
                            remove_from_url_matcher);

  // This is a helper function to GetMatches. Rules triggered by |url_matches|
  // get added to |result| if one of their conditions is fulfilled.
  // |request_data| gets passed to IsFulfilled of the rules' condition sets.
  void AddTriggeredRules(const URLMatches& url_matches,
                         const WebRequestCondition::MatchData& request_data,
                         RuleSet* result) const;

  // Map that tells us which WebRequestRule may match under the condition that
  // the URLMatcherConditionSet::ID was returned by the |url_matcher_|.
  RuleTriggers rule_triggers_;

  // These rules contain condition sets with conditions without URL attributes.
  // Such conditions are not triggered by URL matcher, so we need to test them
  // separately.
  std::set<const WebRequestRule*> rules_with_untriggered_conditions_;

  std::map<ExtensionId, RulesMap> webrequest_rules_;

  url_matcher::URLMatcher url_matcher_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestRulesRegistry);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_RULES_REGISTRY_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_

#include <list>
#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"

namespace base {
class Clock;
class RetainingOneShotTimer;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
struct WebRequestInfo;

namespace declarative_net_request {
struct RequestAction;

class ActionTracker {
 public:
  // The lifespan of matched rules in |rules_matched_| not associated with an
  // active document,
  static constexpr base::TimeDelta kNonActiveTabRuleLifespan = base::Minutes(5);

  explicit ActionTracker(content::BrowserContext* browser_context);
  ~ActionTracker();
  ActionTracker(const ActionTracker& other) = delete;
  ActionTracker& operator=(const ActionTracker& other) = delete;

  // TODO(crbug.com/40115239): Use a task environment to avoid having to set
  // clocks just for tests.

  // Sets a custom Clock to use in tests. |clock| should be owned by the caller
  // of this function.
  void SetClockForTests(const base::Clock* clock);

  // Sets a custom RetainingOneShotTimer to use in tests, which replaces
  // |trim_rules_timer_|.
  void SetTimerForTest(
      std::unique_ptr<base::RetainingOneShotTimer> injected_trim_rules_timer);

  // Disables checking whether a tab ID corresponds to an existing tab when a
  // rule is matched. Used for unit tests where WebContents/actual tabs do not
  // exist.
  void SetCheckTabIdOnRuleMatchForTest(bool check_tab_id);

  // Called whenever a request matches with a rule.
  void OnRuleMatched(const RequestAction& request_action,
                     const WebRequestInfo& request_info);

  // Updates the action count for all tabs for the specified |extension_id|'s
  // extension action. Called when the extension calls setExtensionActionOptions
  // to enable setting the action count as badge text.
  void OnActionCountAsBadgeTextPreferenceEnabled(
      const ExtensionId& extension_id) const;

  // Clears the TrackedInfo for the specified |extension_id| for all tabs.
  // Called when an extension's ruleset is removed.
  void ClearExtensionData(const ExtensionId& extension_id);

  // Clears the TrackedInfo for every extension for the specified |tab_id|.
  // Called when the tab has been closed.
  void ClearTabData(int tab_id);

  // Clears the pending action count for every extension in
  // |pending_navigation_actions_| for the specified |navigation_id|.
  void ClearPendingNavigation(int64_t navigation_id);

  // Called when a main-frame navigation to a different document commits.
  // Updates the TrackedInfo for all extensions for the given |tab_id|.
  void ResetTrackedInfoForTab(int tab_id, int64_t navigation_id);

  // Returns all matched rules for |extension|. If |tab_id| is provided, only
  // rules matched for |tab_id| will be returned.
  std::vector<api::declarative_net_request::MatchedRuleInfo> GetMatchedRules(
      const Extension& extension,
      const std::optional<int>& tab_id,
      const base::Time& min_time_stamp);

  // Returns the number of matched rules in |rules_tracked_| for the given
  // |extension_id| and |tab_id|. If |trim_non_active_rules| is true,
  // TrimRulesFromNonActiveTabs is invoked before returning the matched rule
  // count, similar to GetMatchedRules. Should only be used for tests.
  int GetMatchedRuleCountForTest(const ExtensionId& extension_id,
                                 int tab_id,
                                 bool trim_non_active_rules);

  // Returns the number of matched rules in |pending_navigation_actions_| for
  // the given |extension_id| and |navigation_id|. Should only be used for
  // tests.
  int GetPendingRuleCountForTest(const ExtensionId& extension_id,
                                 int64_t navigation_id);

  // Increments the action count for the given |extension_id| and |tab_id|.
  // A negative value for |increment| will decrement the action count, but the
  // action count will never be less than 0.
  void IncrementActionCountForTab(const ExtensionId& extension_id,
                                  int tab_id,
                                  int increment);

 private:
  // Template key type used for TrackedInfo, specified by an extension_id and
  // another ID.
  template <typename T>
  struct TrackedInfoContextKey {
    TrackedInfoContextKey(ExtensionId extension_id, T secondary_id);
    TrackedInfoContextKey(const TrackedInfoContextKey& other) = delete;
    TrackedInfoContextKey& operator=(const TrackedInfoContextKey& other) =
        delete;
    TrackedInfoContextKey(TrackedInfoContextKey&&);
    TrackedInfoContextKey& operator=(TrackedInfoContextKey&&);

    ExtensionId extension_id;
    T secondary_id;

    bool operator<(const TrackedInfoContextKey& other) const;
  };

  using ExtensionTabIdKey = TrackedInfoContextKey<int>;
  using ExtensionNavigationIdKey = TrackedInfoContextKey<int64_t>;

  // Represents a matched rule. This is used as a lightweight version of
  // api::declarative_net_request::MatchedRuleInfo.
  struct TrackedRule {
    TrackedRule(int rule_id, RulesetID ruleset_id);
    TrackedRule(const TrackedRule& other) = delete;
    TrackedRule& operator=(const TrackedRule& other) = delete;

    const int rule_id;
    const RulesetID ruleset_id;

    // The timestamp for when the rule was matched.
    const base::Time time_stamp;
  };

  // Info tracked for each ExtensionTabIdKey or ExtensionNavigationIdKey.
  struct TrackedInfo {
    TrackedInfo();
    ~TrackedInfo();
    TrackedInfo(const TrackedInfo& other) = delete;
    TrackedInfo& operator=(const TrackedInfo& other) = delete;
    TrackedInfo(TrackedInfo&&);
    TrackedInfo& operator=(TrackedInfo&&);

    // The number of actions matched. Invalid when the corresponding
    // TrackedInfoContextKey has a tab_id of -1. Does not include allow rules.
    size_t action_count = 0;

    // The list of rules matched. Includes allow rules.
    std::list<TrackedRule> matched_rules;
  };

  // Called from OnRuleMatched. Dispatches a OnRuleMatchedDebug event to the
  // observer for the extension specified by |request_action.extension_id|.
  void DispatchOnRuleMatchedDebugIfNeeded(
      const RequestAction& request_action,
      api::declarative_net_request::RequestDetails request_details);

  // For all matched rules attributed to |tab_id|, set their tab ID to the
  // unknown tab ID (-1). Called by ClearTabData and ResetActionCountForTab.
  void TransferRulesOnTabInvalid(int tab_id);

  // Removes all rules in |rules_tracked_| older than
  // |kNonActiveTabRuleLifespan| from non active tabs (i.e. |tab_id| = -1).
  // Called periodically to ensure no rules attributed to the unknown tab ID in
  // |rules_tracked_| are older than |kNonActiveTabRuleLifespan|.
  void TrimRulesFromNonActiveTabs();

  // Schedules TrimRulesFromNonActiveTabs to be run after
  // |kNonActiveTabRuleLifespan|. Called in the constructor and whenever
  // |trim_rules_timer_| gets set.
  void StartTrimRulesTask();

  // Converts an internally represented |tracked_rule| to a MatchedRuleInfo.
  api::declarative_net_request::MatchedRuleInfo CreateMatchedRuleInfo(
      const Extension& extension,
      const TrackedRule& tracked_rule,
      int tab_id) const;

  // A timer to call TrimRulesFromNonActiveTabs with an interval of
  // |kNonActiveTabRuleLifespan|.
  std::unique_ptr<base::RetainingOneShotTimer> trim_rules_timer_ =
      std::make_unique<base::RetainingOneShotTimer>();

  // Maps a pair of (extension ID, tab ID) to the TrackedInfo for that pair.
  std::map<ExtensionTabIdKey, TrackedInfo> rules_tracked_;

  // Maps a pair of (extension ID, navigation ID) to the TrackedInfo matched for
  // the main-frame request associated with the navigation ID in the key. The
  // TrackedInfo is added to |rules_tracked_| once the navigation commits.
  std::map<ExtensionNavigationIdKey, TrackedInfo> pending_navigation_actions_;

  raw_ptr<content::BrowserContext> browser_context_;

  PrefsHelper prefs_helper_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_

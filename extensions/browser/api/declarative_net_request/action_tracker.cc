// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/action_tracker.h"

#include <list>
#include <map>
#include <tuple>
#include <utility>

#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace extensions::declarative_net_request {

namespace {

namespace dnr_api = api::declarative_net_request;

bool IsMainFrameNavigationRequest(const WebRequestInfo& request_info) {
  return request_info.is_navigation_request &&
         request_info.web_request_type == WebRequestResourceType::MAIN_FRAME;
}

// Returns whether a TrackedRule should be recorded on a rule match for the
// extension with the specified |extension_id|.
bool ShouldRecordMatchedRule(content::BrowserContext* browser_context,
                             const ExtensionId& extension_id,
                             int tab_id) {
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  DCHECK(extension);

  const PermissionsData* permissions_data = extension->permissions_data();

  const bool has_feedback_permission = permissions_data->HasAPIPermission(
      mojom::APIPermissionID::kDeclarativeNetRequestFeedback);

  const bool has_active_tab_permission =
      permissions_data->HasAPIPermission(mojom::APIPermissionID::kActiveTab);

  // Always record a matched rule if |extension| has the feedback permission or
  // the request is associated with a tab and |extension| has the activeTab
  // permission.
  return has_feedback_permission ||
         (tab_id != extension_misc::kUnknownTabId && has_active_tab_permission);
}

const base::Clock* g_test_clock = nullptr;

base::Time GetNow() {
  return g_test_clock ? g_test_clock->Now() : base::Time::Now();
}

bool g_check_tab_id_on_rule_match = true;

// Returns the tab ID to use for tracking a matched rule. Any ID corresponding
// to a tab that no longer exists will be mapped to the unknown tab ID. This is
// similar to when a tab is destroyed, its matched rules are re-mapped to the
// unknown tab ID.
int GetTabIdForMatchedRule(content::BrowserContext* browser_context,
                           int request_tab_id) {
  if (!g_check_tab_id_on_rule_match) {
    return request_tab_id;
  }

  DCHECK(ExtensionsBrowserClient::Get());
  return ExtensionsBrowserClient::Get()->IsValidTabId(browser_context,
                                                      request_tab_id)
             ? request_tab_id
             : extension_misc::kUnknownTabId;
}

}  // namespace

// static
constexpr base::TimeDelta ActionTracker::kNonActiveTabRuleLifespan;

ActionTracker::ActionTracker(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      prefs_helper_(*ExtensionPrefs::Get(browser_context_)) {
  StartTrimRulesTask();
}

ActionTracker::~ActionTracker() {
  DCHECK(pending_navigation_actions_.empty());
}

void ActionTracker::SetClockForTests(const base::Clock* clock) {
  g_test_clock = clock;
}

void ActionTracker::SetTimerForTest(
    std::unique_ptr<base::RetainingOneShotTimer> injected_trim_rules_timer) {
  DCHECK(injected_trim_rules_timer);

  trim_rules_timer_ = std::move(injected_trim_rules_timer);
  StartTrimRulesTask();
}

void ActionTracker::SetCheckTabIdOnRuleMatchForTest(bool check_tab_id) {
  g_check_tab_id_on_rule_match = check_tab_id;
}

void ActionTracker::OnRuleMatched(const RequestAction& request_action,
                                  const WebRequestInfo& request_info) {
  const int tab_id =
      GetTabIdForMatchedRule(browser_context_, request_info.frame_data.tab_id);

  dnr_api::RequestDetails request_details = CreateRequestDetails(request_info);
  request_details.tab_id = tab_id;

  DispatchOnRuleMatchedDebugIfNeeded(request_action,
                                     std::move(request_details));

  const ExtensionId& extension_id = request_action.extension_id;
  const bool should_record_rule =
      ShouldRecordMatchedRule(browser_context_, extension_id, tab_id);

  auto add_matched_rule_if_needed = [this, should_record_rule](
                                        TrackedInfo* tracked_info,
                                        const RequestAction& request_action) {
    if (!should_record_rule) {
      return;
    }

    // Restart the timer if it is not running and a matched rule is being added.
    if (!trim_rules_timer_->IsRunning()) {
      trim_rules_timer_->Reset();
    }

    tracked_info->matched_rules.emplace_back(request_action.rule_id,
                                             request_action.ruleset_id);
  };

  // Allow rules do not result in any action being taken on the request, and
  // badge text should only be set for valid tab IDs.
  const bool increment_action_count =
      tab_id != extension_misc::kUnknownTabId &&
      !request_action.IsAllowOrAllowAllRequests();

  if (IsMainFrameNavigationRequest(request_info)) {
    DCHECK(request_info.navigation_id);
    TrackedInfo& pending_info = pending_navigation_actions_[{
        extension_id, *request_info.navigation_id}];
    add_matched_rule_if_needed(&pending_info, request_action);

    if (increment_action_count) {
      pending_info.action_count++;
    }
    return;
  }

  TrackedInfo& tracked_info = rules_tracked_[{extension_id, tab_id}];
  add_matched_rule_if_needed(&tracked_info, request_action);

  if (!increment_action_count) {
    return;
  }

  size_t action_count = ++tracked_info.action_count;
  if (!prefs_helper_.GetUseActionCountAsBadgeText(extension_id)) {
    return;
  }

  DCHECK(ExtensionsAPIClient::Get());
  ExtensionsAPIClient::Get()->UpdateActionCount(browser_context_, extension_id,
                                                tab_id, action_count,
                                                false /* clear_badge_text */);
}

void ActionTracker::OnActionCountAsBadgeTextPreferenceEnabled(
    const ExtensionId& extension_id) const {
  DCHECK(prefs_helper_.GetUseActionCountAsBadgeText(extension_id));

  for (auto it = rules_tracked_.begin(); it != rules_tracked_.end(); ++it) {
    const ExtensionTabIdKey& key = it->first;
    const TrackedInfo& value = it->second;

    if (key.extension_id != extension_id ||
        key.secondary_id == extension_misc::kUnknownTabId) {
      continue;
    }

    ExtensionsAPIClient::Get()->UpdateActionCount(
        browser_context_, extension_id, key.secondary_id /* tab_id */,
        value.action_count, true /* clear_badge_text */);
  }
}

void ActionTracker::ClearExtensionData(const ExtensionId& extension_id) {
  auto compare_by_extension_id = [&extension_id](const auto& it) {
    return it.first.extension_id == extension_id;
  };

  std::erase_if(rules_tracked_, compare_by_extension_id);
  std::erase_if(pending_navigation_actions_, compare_by_extension_id);

  // Stop the timer if there are no more matched rules or pending actions.
  if (rules_tracked_.empty() && pending_navigation_actions_.empty()) {
    trim_rules_timer_->Stop();
  }
}

void ActionTracker::ClearTabData(int tab_id) {
  TransferRulesOnTabInvalid(tab_id);

  auto compare_by_tab_id =
      [&tab_id](const std::pair<const ExtensionTabIdKey, TrackedInfo>& it) {
        bool matches_tab_id = it.first.secondary_id == tab_id;
        DCHECK(!matches_tab_id || it.second.matched_rules.empty());

        return matches_tab_id;
      };

  std::erase_if(rules_tracked_, compare_by_tab_id);
}

void ActionTracker::ClearPendingNavigation(int64_t navigation_id) {
  auto compare_by_navigation_id =
      [navigation_id](
          const std::pair<const ExtensionNavigationIdKey, TrackedInfo>& it) {
        return it.first.secondary_id == navigation_id;
      };

  std::erase_if(pending_navigation_actions_, compare_by_navigation_id);
}

void ActionTracker::ResetTrackedInfoForTab(int tab_id, int64_t navigation_id) {
  DCHECK_NE(tab_id, extension_misc::kUnknownTabId);

  // Since the tab ID for a tracked rule corresponds to the current active
  // document, existing rules for this |tab_id| would point to an inactive
  // document. Therefore the tab IDs for these tracked rules should be set to
  // the unknown tab ID.
  TransferRulesOnTabInvalid(tab_id);

  RulesMonitorService* rules_monitor_service =
      RulesMonitorService::Get(browser_context_);

  DCHECK(rules_monitor_service);

  // Use GetExtensionsWithRulesets() because there may not be an entry for some
  // extensions in |rules_tracked_|. However, the action count should still be
  // surfaced for those extensions if the preference is enabled.
  // TODO(kelvinjiang): Investigate if calling UpdateActionCount for all
  // extensions with rulesets is necessary now that we don't show the action
  // count if it is zero.
  for (const auto& extension_id :
       rules_monitor_service->ruleset_manager()->GetExtensionsWithRulesets()) {
    ExtensionNavigationIdKey navigation_key(extension_id, navigation_id);

    TrackedInfo& tab_info = rules_tracked_[{extension_id, tab_id}];
    DCHECK(tab_info.matched_rules.empty());

    auto iter = pending_navigation_actions_.find({extension_id, navigation_id});
    if (iter != pending_navigation_actions_.end()) {
      tab_info = std::move(iter->second);
    } else {
      // Reset the count and matched rules for the new document.
      tab_info = TrackedInfo();
    }

    if (prefs_helper_.GetUseActionCountAsBadgeText(extension_id)) {
      DCHECK(ExtensionsAPIClient::Get());
      ExtensionsAPIClient::Get()->UpdateActionCount(
          browser_context_, extension_id, tab_id, tab_info.action_count,
          false /* clear_badge_text */);
    }
  }

  // Double check to make sure the pending counts for |navigation_id| are really
  // cleared from |pending_navigation_actions_|.
  ClearPendingNavigation(navigation_id);
}

std::vector<dnr_api::MatchedRuleInfo> ActionTracker::GetMatchedRules(
    const Extension& extension,
    const std::optional<int>& tab_id,
    const base::Time& min_time_stamp) {
  TrimRulesFromNonActiveTabs();

  std::vector<dnr_api::MatchedRuleInfo> matched_rules;
  auto add_to_matched_rules =
      [this, &matched_rules, &min_time_stamp, &extension](
          const std::list<TrackedRule>& tracked_rules, int tab_id) {
        for (const TrackedRule& tracked_rule : tracked_rules) {
          // Filter by the provided |min_time_stamp| for both active and
          // non-active tabs.
          if (tracked_rule.time_stamp >= min_time_stamp) {
            matched_rules.push_back(
                CreateMatchedRuleInfo(extension, tracked_rule, tab_id));
          }
        }
      };

  if (tab_id.has_value()) {
    ExtensionTabIdKey key(extension.id(), *tab_id);

    auto tracked_info = rules_tracked_.find(key);
    if (tracked_info == rules_tracked_.end()) {
      return matched_rules;
    }

    add_to_matched_rules(tracked_info->second.matched_rules, *tab_id);
    return matched_rules;
  }

  // Iterate over all tabs if |tab_id| is not specified.
  for (const auto& it : rules_tracked_) {
    if (it.first.extension_id != extension.id()) {
      continue;
    }

    add_to_matched_rules(it.second.matched_rules, it.first.secondary_id);
  }

  return matched_rules;
}

int ActionTracker::GetMatchedRuleCountForTest(const ExtensionId& extension_id,
                                              int tab_id,
                                              bool trim_non_active_rules) {
  if (trim_non_active_rules) {
    TrimRulesFromNonActiveTabs();
  }

  ExtensionTabIdKey key(extension_id, tab_id);
  auto tracked_info = rules_tracked_.find(key);

  return tracked_info == rules_tracked_.end()
             ? 0
             : tracked_info->second.matched_rules.size();
}

int ActionTracker::GetPendingRuleCountForTest(const ExtensionId& extension_id,
                                              int64_t navigation_id) {
  ExtensionNavigationIdKey key(extension_id, navigation_id);
  auto tracked_info = pending_navigation_actions_.find(key);

  return tracked_info == pending_navigation_actions_.end()
             ? 0
             : tracked_info->second.matched_rules.size();
}

void ActionTracker::IncrementActionCountForTab(const ExtensionId& extension_id,
                                               int tab_id,
                                               int increment) {
  TrackedInfo& tracked_info = rules_tracked_[{extension_id, tab_id}];
  size_t new_action_count =
      std::max<int>(tracked_info.action_count + increment, 0);

  if (tracked_info.action_count == new_action_count) {
    return;
  }

  DCHECK(ExtensionsAPIClient::Get());
  ExtensionsAPIClient::Get()->UpdateActionCount(browser_context_, extension_id,
                                                tab_id, new_action_count,
                                                false /* clear_badge_text */);
  tracked_info.action_count = new_action_count;
}

template <typename T>
ActionTracker::TrackedInfoContextKey<T>::TrackedInfoContextKey(
    ExtensionId extension_id,
    T secondary_id)
    : extension_id(std::move(extension_id)), secondary_id(secondary_id) {}

template <typename T>
ActionTracker::TrackedInfoContextKey<T>::TrackedInfoContextKey(
    ActionTracker::TrackedInfoContextKey<T>&&) = default;

template <typename T>
ActionTracker::TrackedInfoContextKey<T>&
ActionTracker::TrackedInfoContextKey<T>::operator=(
    ActionTracker::TrackedInfoContextKey<T>&&) = default;

template <typename T>
bool ActionTracker::TrackedInfoContextKey<T>::operator<(
    const TrackedInfoContextKey<T>& other) const {
  return std::tie(secondary_id, extension_id) <
         std::tie(other.secondary_id, other.extension_id);
}

ActionTracker::TrackedRule::TrackedRule(int rule_id, RulesetID ruleset_id)
    : rule_id(rule_id), ruleset_id(ruleset_id), time_stamp(GetNow()) {}

ActionTracker::TrackedInfo::TrackedInfo() = default;
ActionTracker::TrackedInfo::~TrackedInfo() = default;
ActionTracker::TrackedInfo::TrackedInfo(ActionTracker::TrackedInfo&&) = default;
ActionTracker::TrackedInfo& ActionTracker::TrackedInfo::operator=(
    ActionTracker::TrackedInfo&&) = default;

void ActionTracker::DispatchOnRuleMatchedDebugIfNeeded(
    const RequestAction& request_action,
    dnr_api::RequestDetails request_details) {
  const ExtensionId& extension_id = request_action.extension_id;
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  DCHECK(extension);

  // Do not dispatch an event if the extension has not registered a listener.
  // |event_router| can be null for some unit tests.
  const EventRouter* event_router = EventRouter::Get(browser_context_);
  const bool has_extension_registered_for_event =
      event_router &&
      event_router->ExtensionHasEventListener(
          extension_id, dnr_api::OnRuleMatchedDebug::kEventName);
  if (!has_extension_registered_for_event) {
    return;
  }

  DCHECK(Manifest::IsUnpackedLocation(extension->location()));

  // Create and dispatch the OnRuleMatchedDebug event.
  dnr_api::MatchedRule matched_rule;
  matched_rule.rule_id = request_action.rule_id;
  matched_rule.ruleset_id =
      GetPublicRulesetID(*extension, request_action.ruleset_id);

  dnr_api::MatchedRuleInfoDebug matched_rule_info_debug;
  matched_rule_info_debug.rule = std::move(matched_rule);
  matched_rule_info_debug.request = std::move(request_details);

  base::Value::List args;
  args.Append(matched_rule_info_debug.ToValue());

  auto event = std::make_unique<Event>(
      events::DECLARATIVE_NET_REQUEST_ON_RULE_MATCHED_DEBUG,
      dnr_api::OnRuleMatchedDebug::kEventName, std::move(args));
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void ActionTracker::TransferRulesOnTabInvalid(int tab_id) {
  DCHECK_NE(tab_id, extension_misc::kUnknownTabId);

  for (auto& [key, value] : rules_tracked_) {
    if (key.secondary_id != tab_id) {
      continue;
    }

    TrackedInfo& unknown_tab_info =
        rules_tracked_[{key.extension_id, extension_misc::kUnknownTabId}];

    // Transfer matched rules for this extension and |tab_id| into the matched
    // rule list for this extension and the unknown tab ID.
    unknown_tab_info.matched_rules.splice(unknown_tab_info.matched_rules.end(),
                                          value.matched_rules);
  }
}

void ActionTracker::TrimRulesFromNonActiveTabs() {
  const base::Time now = GetNow();

  auto older_than_lifespan = [&now](const TrackedRule& tracked_rule) {
    return tracked_rule.time_stamp <= now - kNonActiveTabRuleLifespan;
  };

  for (auto it = rules_tracked_.begin(); it != rules_tracked_.end();) {
    const ExtensionTabIdKey& key = it->first;
    if (key.secondary_id != extension_misc::kUnknownTabId) {
      ++it;
      continue;
    }

    TrackedInfo& tracked_info = it->second;
    std::erase_if(tracked_info.matched_rules, older_than_lifespan);

    if (tracked_info.matched_rules.empty()) {
      it = rules_tracked_.erase(it);
    } else {
      ++it;
    }
  }

  trim_rules_timer_->Reset();
}

void ActionTracker::StartTrimRulesTask() {
  trim_rules_timer_->Start(FROM_HERE, kNonActiveTabRuleLifespan, this,
                           &ActionTracker::TrimRulesFromNonActiveTabs);
}

dnr_api::MatchedRuleInfo ActionTracker::CreateMatchedRuleInfo(
    const Extension& extension,
    const ActionTracker::TrackedRule& tracked_rule,
    int tab_id) const {
  dnr_api::MatchedRule matched_rule;
  matched_rule.rule_id = tracked_rule.rule_id;
  matched_rule.ruleset_id =
      GetPublicRulesetID(extension, tracked_rule.ruleset_id);

  dnr_api::MatchedRuleInfo matched_rule_info;
  matched_rule_info.rule = std::move(matched_rule);
  matched_rule_info.tab_id = tab_id;
  matched_rule_info.time_stamp =
      tracked_rule.time_stamp.InMillisecondsFSinceUnixEpochIgnoringNull();

  return matched_rule_info;
}

}  // namespace extensions::declarative_net_request

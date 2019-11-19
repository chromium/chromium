// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/action_tracker.h"

#include "base/stl_util.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {
namespace declarative_net_request {

namespace dnr_api = api::declarative_net_request;

ActionTracker::ActionTracker(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_prefs_ = ExtensionPrefs::Get(browser_context_);
}

ActionTracker::~ActionTracker() {
  DCHECK(actions_matched_.empty());
}

void ActionTracker::OnRuleMatched(const RequestAction& request_action,
                                  const WebRequestInfo& request_info) {
  const int tab_id = request_info.frame_data.tab_id;
  if (tab_id == extension_misc::kUnknownTabId)
    return;

  DispatchOnRuleMatchedDebugIfNeeded(request_action,
                                     CreateRequestDetails(request_info));

  // Return early since allow rules do not result in any action being taken on
  // the request.
  if (request_action.type == RequestAction::Type::ALLOW)
    return;

  const ExtensionId& extension_id = request_action.extension_id;
  auto key = std::make_pair(extension_id, tab_id);
  int action_count = ++actions_matched_[key];

  if (extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id)) {
    DCHECK(ExtensionsAPIClient::Get());
    ExtensionsAPIClient::Get()->UpdateActionCount(
        browser_context_, extension_id, tab_id, action_count,
        false /* clear_badge_text */);
  }
}

void ActionTracker::OnPreferenceEnabled(const ExtensionId& extension_id) const {
  DCHECK(extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id));

  for (auto it = actions_matched_.begin(); it != actions_matched_.end(); ++it) {
    if (it->first.first != extension_id)
      continue;

    int tab_id = it->first.second;
    int action_count = it->second;
    ExtensionsAPIClient::Get()->UpdateActionCount(
        browser_context_, extension_id, tab_id, action_count,
        true /* clear_badge_text */);
  }
}

void ActionTracker::ClearExtensionData(const ExtensionId& extension_id) {
  auto compare_by_extension_id =
      [&extension_id](const std::pair<ExtensionTabKey, size_t>& it) {
        return it.first.first == extension_id;
      };

  base::EraseIf(actions_matched_, compare_by_extension_id);
}

void ActionTracker::ClearTabData(int tab_id) {
  auto compare_by_tab_id =
      [&tab_id](const std::pair<ExtensionTabKey, size_t>& it) {
        return it.first.second == tab_id;
      };

  base::EraseIf(actions_matched_, compare_by_tab_id);
}

void ActionTracker::ResetActionCountForTab(int tab_id) {
  RulesMonitorService* rules_monitor_service =
      RulesMonitorService::Get(browser_context_);

  DCHECK(rules_monitor_service);
  for (const auto& extension_id :
       rules_monitor_service->extensions_with_rulesets()) {
    auto key = std::make_pair(extension_id, tab_id);
    actions_matched_[key] = 0;

    if (extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id)) {
      DCHECK(ExtensionsAPIClient::Get());
      ExtensionsAPIClient::Get()->UpdateActionCount(
          browser_context_, extension_id, tab_id, 0 /* action_count */,
          false /* clear_badge_text */);
    }
  }
}

void ActionTracker::DispatchOnRuleMatchedDebugIfNeeded(
    const RequestAction& request_action,
    dnr_api::RequestDetails request_details) {
  const ExtensionId& extension_id = request_action.extension_id;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  DCHECK(extension);

  // Do not dispatch an event if the extension has not registered a listener.
  const bool has_extension_registered_for_event =
      EventRouter::Get(browser_context_)
          ->ExtensionHasEventListener(extension_id,
                                      dnr_api::OnRuleMatchedDebug::kEventName);
  if (!has_extension_registered_for_event)
    return;

  DCHECK(Manifest::IsUnpackedLocation(extension->location()));

  // Create and dispatch the OnRuleMatchedDebug event.
  dnr_api::MatchedRule matched_rule;
  matched_rule.rule_id = request_action.rule_id;
  matched_rule.source_type = request_action.source_type;

  dnr_api::MatchedRuleInfoDebug matched_rule_info_debug;
  matched_rule_info_debug.rule = std::move(matched_rule);
  matched_rule_info_debug.request = std::move(request_details);

  auto args = std::make_unique<base::ListValue>();
  args->Append(matched_rule_info_debug.ToValue());

  auto event = std::make_unique<Event>(
      events::DECLARATIVE_NET_REQUEST_ON_RULE_MATCHED_DEBUG,
      dnr_api::OnRuleMatchedDebug::kEventName, std::move(args));
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

}  // namespace declarative_net_request
}  // namespace extensions

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"

#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

namespace dnr_api = api::declarative_net_request;

// Returns whether |extension| can call getMatchedRules for the specified
// |tab_id| and populates |error| if it can't. If no tab ID is specified, then
// the API call is for all tabs.
bool CanCallGetMatchedRules(content::BrowserContext* browser_context,
                            const Extension* extension,
                            std::optional<int> tab_id,
                            std::string* error) {
  bool can_call =
      declarative_net_request::HasDNRFeedbackPermission(extension, tab_id);
  if (!can_call) {
    *error = declarative_net_request::kErrorGetMatchedRulesMissingPermissions;
  }

  return can_call;
}

// Filter the fetched dynamic/session rules by the user provided rule filter.
void FilterRules(std::vector<dnr_api::Rule>& rules,
                 const dnr_api::GetRulesFilter& rule_filter) {
  // Filter the rules by the rule IDs, if provided.
  if (rule_filter.rule_ids) {
    const base::flat_set<int>& rule_ids = *rule_filter.rule_ids;
    std::erase_if(rules, [rule_ids](const auto& rule) {
      return !rule_ids.contains(rule.id);
    });
  }
}

// Returns if the first action in `actions` will intercept the request (i.e.
// block or redirect it).
// Note: If `actions` contains more than one action, then it's guaranteed to be
// modifyHeaders actions which do not intercept the request. See
// DeclarativeNetRequestTestMatchOutcomeFunction::GetActions, which mirrors
// RulesetManager::EvaluateRequestInternal for more details.
bool IsRequestIntercepted(
    const std::vector<declarative_net_request::RequestAction>& actions) {
  return !actions.empty() &&
         (actions[0].IsBlockOrCollapse() || actions[0].IsRedirectOrUpgrade());
}

// Returns the priority of the matching allow action in `actions` or 0 if none
// exists. Note: DeclarativeNetRequestTestMatchOutcomeFunction::GetActions.
// which is based off of RulesetManager::EvaluateRequestInternal, will return
// either no action, a list of modifyheaders actions or a single action of any
// other type. Based on this, only the first action of `actions` need to be
// examined.
uint64_t GetAllowActionPriority(
    const std::vector<declarative_net_request::RequestAction>& actions) {
  uint64_t max_priority = 0;
  if (!actions.empty() && actions[0].IsAllowOrAllowAllRequests()) {
    max_priority = actions[0].index_priority;
  }

  return max_priority;
}

}  // namespace

DeclarativeNetRequestUpdateDynamicRulesFunction::
    DeclarativeNetRequestUpdateDynamicRulesFunction() = default;
DeclarativeNetRequestUpdateDynamicRulesFunction::
    ~DeclarativeNetRequestUpdateDynamicRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestUpdateDynamicRulesFunction::Run() {
  using Params = dnr_api::UpdateDynamicRules::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  std::vector<int> rule_ids_to_remove;
  if (params->options.remove_rule_ids) {
    rule_ids_to_remove = std::move(*params->options.remove_rule_ids);
  }

  std::vector<dnr_api::Rule> rules_to_add;
  if (params->options.add_rules) {
    rules_to_add = std::move(*params->options.add_rules);
  }

  // Early return if there is nothing to do.
  if (rule_ids_to_remove.empty() && rules_to_add.empty()) {
    return RespondNow(NoArguments());
  }

  // Collect rules to add in the Extension Telemetry Service.
  if (!rules_to_add.empty()) {
    ExtensionsBrowserClient::Get()->NotifyExtensionApiDeclarativeNetRequest(
        browser_context(), extension_id(), rules_to_add);
  }

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  DCHECK(extension());

  auto callback = base::BindOnce(
      &DeclarativeNetRequestUpdateDynamicRulesFunction::OnDynamicRulesUpdated,
      this);
  rules_monitor_service->UpdateDynamicRules(
      *extension(), std::move(rule_ids_to_remove), std::move(rules_to_add),
      std::move(callback));
  return RespondLater();
}

void DeclarativeNetRequestUpdateDynamicRulesFunction::OnDynamicRulesUpdated(
    std::optional<std::string> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
}

DeclarativeNetRequestGetDynamicRulesFunction::
    DeclarativeNetRequestGetDynamicRulesFunction() = default;
DeclarativeNetRequestGetDynamicRulesFunction::
    ~DeclarativeNetRequestGetDynamicRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetDynamicRulesFunction::Run() {
  using Params = dnr_api::GetDynamicRules::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  auto source = declarative_net_request::FileBackedRulesetSource::CreateDynamic(
      browser_context(), extension()->id());

  auto read_dynamic_rules = base::BindOnce(
      [](const declarative_net_request::FileBackedRulesetSource& source) {
        return source.ReadJSONRulesUnsafe();
      },
      std::move(source));

  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(read_dynamic_rules),
      base::BindOnce(
          &DeclarativeNetRequestGetDynamicRulesFunction::OnDynamicRulesFetched,
          this, std::move(params).value()));
  return RespondLater();
}

void DeclarativeNetRequestGetDynamicRulesFunction::OnDynamicRulesFetched(
    dnr_api::GetDynamicRules::Params params,
    declarative_net_request::ReadJSONRulesResult read_json_result) {
  using Status = declarative_net_request::ReadJSONRulesResult::Status;

  LogReadDynamicRulesStatus(read_json_result.status);
  DCHECK(read_json_result.status == Status::kSuccess ||
         read_json_result.rules.empty());

  // Unlike errors such as kJSONParseError, which normally denote corruption, a
  // read error is probably a transient error.  Hence raise an error instead of
  // returning an empty list.
  if (read_json_result.status == Status::kFileReadError) {
    Respond(Error(declarative_net_request::kInternalErrorGettingDynamicRules));
    return;
  }

  if (params.filter) {
    FilterRules(read_json_result.rules, *params.filter);
  }

  Respond(ArgumentList(
      dnr_api::GetDynamicRules::Results::Create(read_json_result.rules)));
}

DeclarativeNetRequestUpdateSessionRulesFunction::
    DeclarativeNetRequestUpdateSessionRulesFunction() = default;
DeclarativeNetRequestUpdateSessionRulesFunction::
    ~DeclarativeNetRequestUpdateSessionRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestUpdateSessionRulesFunction::Run() {
  using Params = dnr_api::UpdateSessionRules::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  std::vector<int> rule_ids_to_remove;
  if (params->options.remove_rule_ids) {
    rule_ids_to_remove = std::move(*params->options.remove_rule_ids);
  }

  std::vector<dnr_api::Rule> rules_to_add;
  if (params->options.add_rules) {
    rules_to_add = std::move(*params->options.add_rules);
  }

  // Early return if there is nothing to do.
  if (rule_ids_to_remove.empty() && rules_to_add.empty()) {
    return RespondNow(NoArguments());
  }

  // Collect rules to add in the Extension Telemetry Service.
  if (!rules_to_add.empty()) {
    ExtensionsBrowserClient::Get()->NotifyExtensionApiDeclarativeNetRequest(
        browser_context(), extension_id(), rules_to_add);
  }

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  rules_monitor_service->UpdateSessionRules(
      *extension(), std::move(rule_ids_to_remove), std::move(rules_to_add),
      base::BindOnce(&DeclarativeNetRequestUpdateSessionRulesFunction::
                         OnSessionRulesUpdated,
                     this));
  return RespondLater();
}

void DeclarativeNetRequestUpdateSessionRulesFunction::OnSessionRulesUpdated(
    std::optional<std::string> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
}

DeclarativeNetRequestGetSessionRulesFunction::
    DeclarativeNetRequestGetSessionRulesFunction() = default;
DeclarativeNetRequestGetSessionRulesFunction::
    ~DeclarativeNetRequestGetSessionRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetSessionRulesFunction::Run() {
  using Params = dnr_api::GetSessionRules::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);

  auto rules = rules_monitor_service->GetSessionRules(extension_id());

  if (params->filter) {
    FilterRules(rules, *params->filter);
  }

  return RespondNow(
      ArgumentList(dnr_api::GetSessionRules::Results::Create(rules)));
}

DeclarativeNetRequestUpdateEnabledRulesetsFunction::
    DeclarativeNetRequestUpdateEnabledRulesetsFunction() = default;
DeclarativeNetRequestUpdateEnabledRulesetsFunction::
    ~DeclarativeNetRequestUpdateEnabledRulesetsFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestUpdateEnabledRulesetsFunction::Run() {
  using Params = dnr_api::UpdateEnabledRulesets::Params;
  using RulesetID = declarative_net_request::RulesetID;
  using DNRManifestData = declarative_net_request::DNRManifestData;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  std::set<RulesetID> ids_to_disable;
  std::set<RulesetID> ids_to_enable;
  const DNRManifestData::ManifestIDToRulesetMap& public_id_map =
      DNRManifestData::GetManifestIDToRulesetMap(*extension());

  if (params->options.enable_ruleset_ids) {
    for (const std::string& public_id_to_enable :
         *params->options.enable_ruleset_ids) {
      auto it = public_id_map.find(public_id_to_enable);
      if (it == public_id_map.end()) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            declarative_net_request::kInvalidRulesetIDError,
            public_id_to_enable)));
      }

      ids_to_enable.insert(it->second->id);
    }
  }

  if (params->options.disable_ruleset_ids) {
    for (const std::string& public_id_to_disable :
         *params->options.disable_ruleset_ids) {
      auto it = public_id_map.find(public_id_to_disable);
      if (it == public_id_map.end()) {
        return RespondNow(Error(ErrorUtils::FormatErrorMessage(
            declarative_net_request::kInvalidRulesetIDError,
            public_id_to_disable)));
      }

      // |ruleset_ids_to_enable| takes priority over |ruleset_ids_to_disable|.
      RulesetID id = it->second->id;
      if (base::Contains(ids_to_enable, id)) {
        continue;
      }

      ids_to_disable.insert(id);
    }
  }

  if (ids_to_enable.empty() && ids_to_disable.empty()) {
    return RespondNow(NoArguments());
  }

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  DCHECK(extension());

  rules_monitor_service->UpdateEnabledStaticRulesets(
      *extension(), std::move(ids_to_disable), std::move(ids_to_enable),
      base::BindOnce(&DeclarativeNetRequestUpdateEnabledRulesetsFunction::
                         OnEnabledStaticRulesetsUpdated,
                     this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeclarativeNetRequestUpdateEnabledRulesetsFunction::
    OnEnabledStaticRulesetsUpdated(std::optional<std::string> error) {
  if (error) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
}

DeclarativeNetRequestGetEnabledRulesetsFunction::
    DeclarativeNetRequestGetEnabledRulesetsFunction() = default;
DeclarativeNetRequestGetEnabledRulesetsFunction::
    ~DeclarativeNetRequestGetEnabledRulesetsFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetEnabledRulesetsFunction::Run() {
  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);

  std::vector<std::string> public_ids;
  declarative_net_request::CompositeMatcher* matcher =
      rules_monitor_service->ruleset_manager()->GetMatcherForExtension(
          extension_id());
  if (matcher) {
    DCHECK(extension());
    public_ids = GetPublicRulesetIDs(*extension(), *matcher);

    // Exclude any reserved ruleset IDs since they would correspond to
    // non-static rulesets.
    std::erase_if(public_ids, [](const std::string& id) {
      DCHECK(!id.empty());
      return id[0] == declarative_net_request::kReservedRulesetIDPrefix;
    });
  }

  return RespondNow(
      ArgumentList(dnr_api::GetEnabledRulesets::Results::Create(public_ids)));
}

DeclarativeNetRequestUpdateStaticRulesFunction::
    DeclarativeNetRequestUpdateStaticRulesFunction() = default;
DeclarativeNetRequestUpdateStaticRulesFunction::
    ~DeclarativeNetRequestUpdateStaticRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestUpdateStaticRulesFunction::Run() {
  using Params = dnr_api::UpdateStaticRules::Params;
  using DNRManifestData = declarative_net_request::DNRManifestData;
  using RulesMonitorService = declarative_net_request::RulesMonitorService;
  using RuleIdsToUpdate = declarative_net_request::PrefsHelper::RuleIdsToUpdate;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  const DNRManifestData::ManifestIDToRulesetMap& public_id_map =
      DNRManifestData::GetManifestIDToRulesetMap(*extension());
  auto it = public_id_map.find(params->options.ruleset_id);
  if (it == public_id_map.end()) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        declarative_net_request::kInvalidRulesetIDError,
        params->options.ruleset_id)));
  }

  RuleIdsToUpdate rule_ids_to_update(params->options.disable_rule_ids,
                                     params->options.enable_rule_ids);

  if (rule_ids_to_update.Empty()) {
    return RespondNow(NoArguments());
  }

  auto* rules_monitor_service = RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  DCHECK(extension());

  rules_monitor_service->UpdateStaticRules(
      *extension(), it->second->id, std::move(rule_ids_to_update),
      base::BindOnce(
          &DeclarativeNetRequestUpdateStaticRulesFunction::OnStaticRulesUpdated,
          this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeclarativeNetRequestUpdateStaticRulesFunction::OnStaticRulesUpdated(
    std::optional<std::string> error) {
  if (error) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
}

DeclarativeNetRequestGetDisabledRuleIdsFunction::
    DeclarativeNetRequestGetDisabledRuleIdsFunction() = default;
DeclarativeNetRequestGetDisabledRuleIdsFunction::
    ~DeclarativeNetRequestGetDisabledRuleIdsFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetDisabledRuleIdsFunction::Run() {
  using Params = dnr_api::GetDisabledRuleIds::Params;
  using RulesetID = declarative_net_request::RulesetID;
  using DNRManifestData = declarative_net_request::DNRManifestData;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  const DNRManifestData::ManifestIDToRulesetMap& public_id_map =
      DNRManifestData::GetManifestIDToRulesetMap(*extension());
  auto it = public_id_map.find(params->options.ruleset_id);
  if (it == public_id_map.end()) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        declarative_net_request::kInvalidRulesetIDError,
        params->options.ruleset_id)));
  }
  RulesetID ruleset_id = it->second->id;

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  DCHECK(extension());

  rules_monitor_service->GetDisabledRuleIds(
      *extension(), std::move(ruleset_id),
      base::BindOnce(&DeclarativeNetRequestGetDisabledRuleIdsFunction::
                         OnDisabledRuleIdsRead,
                     this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeclarativeNetRequestGetDisabledRuleIdsFunction::OnDisabledRuleIdsRead(
    std::vector<int> disabled_rule_ids) {
  Respond(ArgumentList(
      dnr_api::GetDisabledRuleIds::Results::Create(disabled_rule_ids)));
}

// static
bool
    DeclarativeNetRequestGetMatchedRulesFunction::disable_throttling_for_test_ =
        false;

DeclarativeNetRequestGetMatchedRulesFunction::
    DeclarativeNetRequestGetMatchedRulesFunction() = default;
DeclarativeNetRequestGetMatchedRulesFunction::
    ~DeclarativeNetRequestGetMatchedRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetMatchedRulesFunction::Run() {
  using Params = dnr_api::GetMatchedRules::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  std::optional<int> tab_id;
  base::Time min_time_stamp = base::Time::Min();

  if (params->filter) {
    if (params->filter->tab_id) {
      tab_id = *params->filter->tab_id;
    }

    if (params->filter->min_time_stamp) {
      min_time_stamp = base::Time::FromMillisecondsSinceUnixEpoch(
          *params->filter->min_time_stamp);
    }
  }

  // Return an error if an invalid tab ID is specified. The unknown tab ID is
  // valid as it would cause the API call to return all rules matched that were
  // not associated with any currently open tabs.
  if (tab_id && *tab_id != extension_misc::kUnknownTabId &&
      !ExtensionsBrowserClient::Get()->IsValidTabId(browser_context(),
                                                    *tab_id)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        declarative_net_request::kTabNotFoundError,
        base::NumberToString(*tab_id))));
  }

  std::string permission_error;
  if (!CanCallGetMatchedRules(browser_context(), extension(), tab_id,
                              &permission_error)) {
    return RespondNow(Error(permission_error));
  }

  declarative_net_request::RulesMonitorService* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);

  declarative_net_request::ActionTracker& action_tracker =
      rules_monitor_service->action_tracker();

  dnr_api::RulesMatchedDetails details;
  details.rules_matched_info =
      action_tracker.GetMatchedRules(*extension(), tab_id, min_time_stamp);

  return RespondNow(
      ArgumentList(dnr_api::GetMatchedRules::Results::Create(details)));
}

void DeclarativeNetRequestGetMatchedRulesFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config limit = {
      dnr_api::MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL,
      base::Minutes(dnr_api::GETMATCHEDRULES_QUOTA_INTERVAL)};

  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      limit, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL"));
}

bool DeclarativeNetRequestGetMatchedRulesFunction::ShouldSkipQuotaLimiting()
    const {
  return user_gesture() || disable_throttling_for_test_;
}

DeclarativeNetRequestSetExtensionActionOptionsFunction::
    DeclarativeNetRequestSetExtensionActionOptionsFunction() = default;
DeclarativeNetRequestSetExtensionActionOptionsFunction::
    ~DeclarativeNetRequestSetExtensionActionOptionsFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestSetExtensionActionOptionsFunction::Run() {
  using Params = dnr_api::SetExtensionActionOptions::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  declarative_net_request::RulesMonitorService* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);

  declarative_net_request::PrefsHelper helper(
      *ExtensionPrefs::Get(browser_context()));
  declarative_net_request::ActionTracker& action_tracker =
      rules_monitor_service->action_tracker();

  bool use_action_count_as_badge_text =
      helper.GetUseActionCountAsBadgeText(extension_id());

  if (params->options.display_action_count_as_badge_text &&
      *params->options.display_action_count_as_badge_text !=
          use_action_count_as_badge_text) {
    use_action_count_as_badge_text =
        *params->options.display_action_count_as_badge_text;
    helper.SetUseActionCountAsBadgeText(extension_id(),
                                        use_action_count_as_badge_text);

    // If the preference is switched on, update the extension's badge text
    // with the number of actions matched for this extension. Otherwise, clear
    // the action count for the extension's icon and show the default badge
    // text if set.
    if (use_action_count_as_badge_text) {
      action_tracker.OnActionCountAsBadgeTextPreferenceEnabled(extension_id());
    } else {
      DCHECK(ExtensionsAPIClient::Get());
      ExtensionsAPIClient::Get()->ClearActionCount(browser_context(),
                                                   *extension());
    }
  }

  if (params->options.tab_update) {
    if (!use_action_count_as_badge_text) {
      return RespondNow(
          Error(declarative_net_request::
                    kIncrementActionCountWithoutUseAsBadgeTextError));
    }

    const auto& update_options = *params->options.tab_update;
    int tab_id = update_options.tab_id;

    if (!ExtensionsBrowserClient::Get()->IsValidTabId(browser_context(),
                                                      tab_id)) {
      return RespondNow(Error(ErrorUtils::FormatErrorMessage(
          declarative_net_request::kTabNotFoundError,
          base::NumberToString(tab_id))));
    }

    action_tracker.IncrementActionCountForTab(extension_id(), tab_id,
                                              update_options.increment);
  }
  return RespondNow(NoArguments());
}

DeclarativeNetRequestIsRegexSupportedFunction::
    DeclarativeNetRequestIsRegexSupportedFunction() = default;
DeclarativeNetRequestIsRegexSupportedFunction::
    ~DeclarativeNetRequestIsRegexSupportedFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestIsRegexSupportedFunction::Run() {
  using Params = dnr_api::IsRegexSupported::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  bool is_case_sensitive = params->regex_options.is_case_sensitive
                               ? *params->regex_options.is_case_sensitive
                               : true;
  bool require_capturing = params->regex_options.require_capturing
                               ? *params->regex_options.require_capturing
                               : false;
  re2::RE2 regex(params->regex_options.regex,
                 declarative_net_request::CreateRE2Options(is_case_sensitive,
                                                           require_capturing));

  dnr_api::IsRegexSupportedResult result;
  if (regex.ok()) {
    result.is_supported = true;
  } else {
    result.is_supported = false;
    result.reason = regex.error_code() == re2::RE2::ErrorPatternTooLarge
                        ? dnr_api::UnsupportedRegexReason::kMemoryLimitExceeded
                        : dnr_api::UnsupportedRegexReason::kSyntaxError;
  }

  return RespondNow(
      ArgumentList(dnr_api::IsRegexSupported::Results::Create(result)));
}

DeclarativeNetRequestGetAvailableStaticRuleCountFunction::
    DeclarativeNetRequestGetAvailableStaticRuleCountFunction() = default;
DeclarativeNetRequestGetAvailableStaticRuleCountFunction::
    ~DeclarativeNetRequestGetAvailableStaticRuleCountFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetAvailableStaticRuleCountFunction::Run() {
  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);

  declarative_net_request::CompositeMatcher* composite_matcher =
      rules_monitor_service->ruleset_manager()->GetMatcherForExtension(
          extension_id());

  // First get the total enabled static rule count for the extension.
  size_t enabled_static_rule_count =
      GetEnabledStaticRuleCount(composite_matcher);
  size_t static_rule_limit =
      static_cast<size_t>(declarative_net_request::GetMaximumRulesPerRuleset());
  DCHECK_LE(enabled_static_rule_count, static_rule_limit);

  const declarative_net_request::GlobalRulesTracker& global_rules_tracker =
      rules_monitor_service->global_rules_tracker();

  size_t available_allocation =
      global_rules_tracker.GetAvailableAllocation(extension_id());
  size_t guaranteed_static_minimum =
      declarative_net_request::GetStaticGuaranteedMinimumRuleCount();

  // If an extension's rule count is below the guaranteed minimum, include the
  // difference.
  size_t available_static_rule_count = 0;
  if (enabled_static_rule_count < guaranteed_static_minimum) {
    available_static_rule_count =
        (guaranteed_static_minimum - enabled_static_rule_count) +
        available_allocation;
  } else {
    size_t used_global_allocation =
        enabled_static_rule_count - guaranteed_static_minimum;
    DCHECK_GE(available_allocation, used_global_allocation);

    available_static_rule_count = available_allocation - used_global_allocation;
  }

  // Ensure conversion to int below doesn't underflow.
  DCHECK_LE(available_static_rule_count,
            static_cast<size_t>(std::numeric_limits<int>::max()));
  return RespondNow(
      WithArguments(static_cast<int>(available_static_rule_count)));
}

DeclarativeNetRequestTestMatchOutcomeFunction::
    DeclarativeNetRequestTestMatchOutcomeFunction() = default;
DeclarativeNetRequestTestMatchOutcomeFunction::
    ~DeclarativeNetRequestTestMatchOutcomeFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestTestMatchOutcomeFunction::Run() {
  using Params = dnr_api::TestMatchOutcome::Params;

  auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  // Create a RequestParams for the pretend request.

  GURL url = GURL(params->request.url);
  if (!url.is_valid()) {
    return RespondNow(Error(declarative_net_request::kInvalidTestURLError));
  }

  url::Origin initiator;
  if (params->request.initiator) {
    GURL initiator_url = GURL(*params->request.initiator);
    if (!initiator_url.is_valid()) {
      return RespondNow(
          Error(declarative_net_request::kInvalidTestInitiatorError));
    }
    initiator = url::Origin::Create(std::move(initiator_url));
  }

  int tab_id = params->request.tab_id ? *params->request.tab_id
                                      : extension_misc::kUnknownTabId;
  if (tab_id < extension_misc::kUnknownTabId) {
    return RespondNow(Error(declarative_net_request::kInvalidTestTabIdError));
  }

  auto method = params->request.method == dnr_api::RequestMethod::kNone
                    ? dnr_api::RequestMethod::kGet
                    : params->request.method;

  // If enabled, parse response headers from function args into
  // `response_headers`.
  // Note: If headers are not specified in function args, then
  // `response_headers` is set to an empty header object.
  scoped_refptr<const net::HttpResponseHeaders> response_headers = nullptr;
  if (base::FeatureList::IsEnabled(
          extensions_features::kDeclarativeNetRequestResponseHeaderMatching)) {
    std::string parse_header_error;
    response_headers =
        ParseHeaders(params->request.response_headers, parse_header_error);
    if (!parse_header_error.empty()) {
      return RespondNow(Error(parse_header_error));
    }

    DCHECK(response_headers);
  }

  declarative_net_request::RequestParams request_params(
      url, initiator, params->request.type, method, tab_id, response_headers);

  // Set up the rule matcher.

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  DCHECK(extension());

  declarative_net_request::CompositeMatcher* matcher =
      rules_monitor_service->ruleset_manager()->GetMatcherForExtension(
          extension_id());
  if (!matcher) {
    return RespondNow(ArgumentList(dnr_api::TestMatchOutcome::Results::Create(
        dnr_api::TestMatchOutcomeResult())));
  }

  // Determine if the extension has permission to redirect the request.
  auto web_request_resource_type =
      declarative_net_request::GetWebRequestResourceType(params->request.type);
  PermissionsData::PageAccess page_access =
      WebRequestPermissions::CanExtensionAccessURL(
          PermissionHelper::Get(browser_context()), extension_id(), url, tab_id,
          /*crosses_incognito=*/false,
          WebRequestPermissions::HostPermissionsCheck::
              REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          initiator, web_request_resource_type);

  // First, match against rules which operate on the request before it would be
  // sent.
  std::vector<declarative_net_request::RequestAction> before_request_actions =
      GetActions(
          *matcher, request_params,
          declarative_net_request::RulesetMatchingStage::kOnBeforeRequest,
          page_access);

  // Stop here if response headers should not be matched or the request is
  // intercepted before it is sent.
  if (!response_headers || IsRequestIntercepted(before_request_actions)) {
    return RespondNow(
        ArgumentList(CreateMatchedRulesFromActions(before_request_actions)));
  }

  DCHECK(response_headers);

  // Match against rules which match with response headers.
  std::vector<declarative_net_request::RequestAction> headers_received_actions =
      GetActions(
          *matcher, request_params,
          declarative_net_request::RulesetMatchingStage::kOnHeadersReceived,
          page_access);

  // Same as in production, erase any actions from `before_request_actions` with
  // lower priority than the max allow priority from `headers_received_actions`.
  // Note that `request_params.max_priority_allow_action` means that reusing
  // `request_params` when calling `GetActions` for kOnHeadersReceived will only
  // return actions with a higher priority than the max allow priority from
  // kOnBeforeRequest.
  uint64_t headers_received_allow_priority =
      GetAllowActionPriority(headers_received_actions);
  std::erase_if(before_request_actions,
                [headers_received_allow_priority](
                    const declarative_net_request::RequestAction& action) {
                  return action.index_priority <
                         headers_received_allow_priority;
                });

  // At this point, the set of matching request actions for the test request
  // would be {before_request_actions, headers_received_actions}. If one list of
  // actions is empty (which means no actions were applied onto the request from
  // that request phase), return the other.
  if (before_request_actions.empty() ||
      IsRequestIntercepted(headers_received_actions)) {
    return RespondNow(
        ArgumentList(CreateMatchedRulesFromActions(headers_received_actions)));
  }
  if (headers_received_actions.empty()) {
    return RespondNow(
        ArgumentList(CreateMatchedRulesFromActions(before_request_actions)));
  }

  // At this point, if both lists are non-empty, then either:
  // - both lists contain the same max priority allow action matched in
  //   OnBeforeRequest. Return either list.
  // - one list contains an allow action, and the other contains modify header
  //   actions. In this case, return the list with modify header actions. This
  //   mimics the logic in CompositeMatcher::GetModifyHeadersActions where only
  //   modify header actions above the max allow rule's priority are returned.
  // - both lists contain modify header actions. In this case, merge them and
  //   sort in descending order of priority. The merging is done since each
  //   action is relevant to the request and shouldn't be ignored, and the
  //   sorting reflects CompositeMatcher::GetModifyHeadersActions which returns
  //   a list of actions in sorted order.
  DCHECK(!before_request_actions.empty());
  DCHECK(!headers_received_actions.empty());
  if (before_request_actions[0].IsAllowOrAllowAllRequests()) {
    return RespondNow(
        ArgumentList(CreateMatchedRulesFromActions(headers_received_actions)));
  }
  if (headers_received_actions[0].IsAllowOrAllowAllRequests()) {
    return RespondNow(
        ArgumentList(CreateMatchedRulesFromActions(before_request_actions)));
  }

  std::vector<declarative_net_request::RequestAction> merged_actions;
  merged_actions.reserve(before_request_actions.size() +
                         headers_received_actions.size());
  merged_actions.insert(merged_actions.end(),
                        std::make_move_iterator(before_request_actions.begin()),
                        std::make_move_iterator(before_request_actions.end()));
  merged_actions.insert(
      merged_actions.end(),
      std::make_move_iterator(headers_received_actions.begin()),
      std::make_move_iterator(headers_received_actions.end()));
  std::sort(merged_actions.begin(), merged_actions.end(), std::greater<>());

  return RespondNow(
      ArgumentList(CreateMatchedRulesFromActions(merged_actions)));
}

scoped_refptr<const net::HttpResponseHeaders>
DeclarativeNetRequestTestMatchOutcomeFunction::ParseHeaders(
    std::optional<TestResponseHeaders>& test_headers,
    std::string& error) const {
  net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1), "200");

  if (test_headers.has_value()) {
    for (const auto [header, values] : test_headers->additional_properties) {
      if (!net::HttpUtil::IsValidHeaderName(header)) {
        error = ErrorUtils::FormatErrorMessage(
            declarative_net_request::kInvalidResponseHeaderNameError, header);
        return nullptr;
      }

      // Header values must be specified as a list.
      if (!values.is_list()) {
        error = ErrorUtils::FormatErrorMessage(
            declarative_net_request::kInvalidResponseHeaderObjectError, header);
        return nullptr;
      }

      // Assume an empty string as a header value if an empty list is specified.
      if (values.GetList().empty()) {
        builder.AddHeader(header, "");
        continue;
      }
      for (const auto& value : values.GetList()) {
        // Check that header values are valid strings.
        if (!value.is_string() ||
            !net::HttpUtil::IsValidHeaderName(value.GetString())) {
          error = ErrorUtils::FormatErrorMessage(
              declarative_net_request::kInvalidResponseHeaderValueError,
              header);
          return nullptr;
        }
        builder.AddHeader(header, value.GetString());
      }
    }
  }

  return builder.Build();
}

base::Value::List
DeclarativeNetRequestTestMatchOutcomeFunction::CreateMatchedRulesFromActions(
    const std::vector<declarative_net_request::RequestAction>& actions) const {
  dnr_api::TestMatchOutcomeResult result;
  std::vector<dnr_api::MatchedRule> matched_rules;

  for (auto& action : actions) {
    dnr_api::MatchedRule match;
    match.rule_id = action.rule_id;
    match.ruleset_id = GetPublicRulesetID(*extension(), action.ruleset_id);
    matched_rules.push_back(std::move(match));
  }

  result.matched_rules = std::move(matched_rules);
  return dnr_api::TestMatchOutcome::Results::Create(result);
}

std::vector<declarative_net_request::RequestAction>
DeclarativeNetRequestTestMatchOutcomeFunction::GetActions(
    const declarative_net_request::CompositeMatcher& matcher,
    const declarative_net_request::RequestParams& params,
    declarative_net_request::RulesetMatchingStage stage,
    PermissionsData::PageAccess page_access) const {
  // TODO(crbug.com/343503170): The logic here is very similar to that of
  // `RulesetManager::EvaluateRequestInternal` except this is for a single
  // extension. One way to DRY this up is to put this logic in utils, and pass
  // in function objects for matching a singular action and matching modify
  // headers actions.

  using RequestAction = declarative_net_request::RequestAction;
  std::vector<RequestAction> actions;

  std::optional<RequestAction> action =
      matcher.GetAction(params, stage, page_access).action;

  if (action) {
    bool is_request_modifying_action = !action->IsAllowOrAllowAllRequests();
    actions.push_back(std::move(*action));

    if (is_request_modifying_action) {
      return actions;
    }
  }

  std::vector<RequestAction> modify_headers_actions =
      matcher.GetModifyHeadersActions(params, stage);

  if (!modify_headers_actions.empty()) {
    return modify_headers_actions;
  }

  return actions;
}

}  // namespace extensions

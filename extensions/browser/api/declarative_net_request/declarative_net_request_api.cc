// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/declarative_net_request_prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
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
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace {

namespace dnr_api = api::declarative_net_request;

// Returns whether |extension| can call getMatchedRules for the specified
// |tab_id| and populates |error| if it can't. If no tab ID is specified, then
// the API call is for all tabs.
bool CanCallGetMatchedRules(content::BrowserContext* browser_context,
                            const Extension* extension,
                            absl::optional<int> tab_id,
                            std::string* error) {
  bool can_call =
      declarative_net_request::HasDNRFeedbackPermission(extension, tab_id);
  if (!can_call)
    *error = declarative_net_request::kErrorGetMatchedRulesMissingPermissions;

  return can_call;
}

// Filter the fetched dynamic/session rules by the user provided rule filter.
void FilterRules(std::vector<dnr_api::Rule>& rules,
                 const dnr_api::GetRulesFilter& rule_filter) {
  // Filter the rules by the rule IDs, if provided.
  if (rule_filter.rule_ids) {
    const base::flat_set<int>& rule_ids = *rule_filter.rule_ids;
    base::EraseIf(rules, [rule_ids](const auto& rule) {
      return !rule_ids.contains(rule.id);
    });
  }
}

}  // namespace

DeclarativeNetRequestUpdateDynamicRulesFunction::
    DeclarativeNetRequestUpdateDynamicRulesFunction() = default;
DeclarativeNetRequestUpdateDynamicRulesFunction::
    ~DeclarativeNetRequestUpdateDynamicRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestUpdateDynamicRulesFunction::Run() {
  using Params = dnr_api::UpdateDynamicRules::Params;

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  std::vector<int> rule_ids_to_remove;
  if (params->options.remove_rule_ids)
    rule_ids_to_remove = std::move(*params->options.remove_rule_ids);

  std::vector<api::declarative_net_request::Rule> rules_to_add;
  if (params->options.add_rules)
    rules_to_add = std::move(*params->options.add_rules);

  // Early return if there is nothing to do.
  if (rule_ids_to_remove.empty() && rules_to_add.empty())
    return RespondNow(NoArguments());

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
    absl::optional<std::string> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error)
    Respond(Error(std::move(*error)));
  else
    Respond(NoArguments());
}

DeclarativeNetRequestGetDynamicRulesFunction::
    DeclarativeNetRequestGetDynamicRulesFunction() = default;
DeclarativeNetRequestGetDynamicRulesFunction::
    ~DeclarativeNetRequestGetDynamicRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetDynamicRulesFunction::Run() {
  using Params = dnr_api::GetDynamicRules::Params;

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

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
          this, std::move(params)));
  return RespondLater();
}

void DeclarativeNetRequestGetDynamicRulesFunction::OnDynamicRulesFetched(
    absl::optional<dnr_api::GetDynamicRules::Params> params,
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

  if (params->filter) {
    FilterRules(read_json_result.rules, *params->filter);
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

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  std::vector<int> rule_ids_to_remove;
  if (params->options.remove_rule_ids)
    rule_ids_to_remove = std::move(*params->options.remove_rule_ids);

  std::vector<api::declarative_net_request::Rule> rules_to_add;
  if (params->options.add_rules)
    rules_to_add = std::move(*params->options.add_rules);

  // Early return if there is nothing to do.
  if (rule_ids_to_remove.empty() && rules_to_add.empty())
    return RespondNow(NoArguments());

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
    absl::optional<std::string> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error)
    Respond(Error(std::move(*error)));
  else
    Respond(NoArguments());
}

DeclarativeNetRequestGetSessionRulesFunction::
    DeclarativeNetRequestGetSessionRulesFunction() = default;
DeclarativeNetRequestGetSessionRulesFunction::
    ~DeclarativeNetRequestGetSessionRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetSessionRulesFunction::Run() {
  using Params = dnr_api::GetSessionRules::Params;

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

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

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

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
      if (base::Contains(ids_to_enable, id))
        continue;

      ids_to_disable.insert(id);
    }
  }

  if (ids_to_enable.empty() && ids_to_disable.empty())
    return RespondNow(NoArguments());

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
    OnEnabledStaticRulesetsUpdated(absl::optional<std::string> error) {
  if (error)
    Respond(Error(std::move(*error)));
  else
    Respond(NoArguments());
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
    base::EraseIf(public_ids, [](const std::string& id) {
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
  using RuleIdsToUpdate = declarative_net_request::
      DeclarativeNetRequestPrefsHelper::RuleIdsToUpdate;

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

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
    absl::optional<std::string> error) {
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

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

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

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  absl::optional<int> tab_id;
  base::Time min_time_stamp = base::Time::Min();

  if (params->filter) {
    if (params->filter->tab_id)
      tab_id = *params->filter->tab_id;

    if (params->filter->min_time_stamp)
      min_time_stamp = base::Time::FromJsTime(*params->filter->min_time_stamp);
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

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  declarative_net_request::RulesMonitorService* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  declarative_net_request::ActionTracker& action_tracker =
      rules_monitor_service->action_tracker();

  bool use_action_count_as_badge_text =
      prefs->GetDNRUseActionCountAsBadgeText(extension_id());

  if (params->options.display_action_count_as_badge_text &&
      *params->options.display_action_count_as_badge_text !=
          use_action_count_as_badge_text) {
    use_action_count_as_badge_text =
        *params->options.display_action_count_as_badge_text;
    prefs->SetDNRUseActionCountAsBadgeText(extension_id(),
                                           use_action_count_as_badge_text);

    // If the preference is switched on, update the extension's badge text
    // with the number of actions matched for this extension. Otherwise, clear
    // the action count for the extension's icon and show the default badge
    // text if set.
    if (use_action_count_as_badge_text)
      action_tracker.OnActionCountAsBadgeTextPreferenceEnabled(extension_id());
    else {
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

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

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

  std::u16string error;
  absl::optional<Params> params = Params::Create(args(), error);
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

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
  declarative_net_request::RequestParams request_params(
      url, initiator, params->request.type, method, tab_id);

  // Set up the rule matcher.

  dnr_api::TestMatchOutcomeResult result;

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  DCHECK(extension());

  declarative_net_request::CompositeMatcher* matcher =
      rules_monitor_service->ruleset_manager()->GetMatcherForExtension(
          extension_id());
  if (!matcher) {
    return RespondNow(
        ArgumentList(dnr_api::TestMatchOutcome::Results::Create(result)));
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

  // Check for "before request" matches (e.g. allow/block rules).
  declarative_net_request::CompositeMatcher::ActionInfo before_request_action =
      matcher->GetBeforeRequestAction(request_params, page_access);
  if (before_request_action.action) {
    dnr_api::MatchedRule match;
    match.rule_id = before_request_action.action->rule_id;
    match.ruleset_id = GetPublicRulesetID(
        *extension(), before_request_action.action->ruleset_id);
    result.matched_rules.push_back(std::move(match));
  } else {
    // If none found, check for modify header matches.
    for (auto& action : matcher->GetModifyHeadersActions(request_params)) {
      dnr_api::MatchedRule match;
      match.rule_id = action.rule_id;
      match.ruleset_id = GetPublicRulesetID(*extension(), action.ruleset_id);
      result.matched_rules.push_back(std::move(match));
    }
  }

  return RespondNow(
      ArgumentList(dnr_api::TestMatchOutcome::Results::Create(result)));
}

}  // namespace extensions

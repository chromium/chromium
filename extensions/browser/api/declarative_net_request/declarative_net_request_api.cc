// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

namespace dnr_api = api::declarative_net_request;

// Returns whether |extension| can call getMatchedRules for the specified
// |tab_id| and populates |error| if it can't. If no tab ID is specified, then
// the API call is for all tabs.
bool CanCallGetMatchedRules(content::BrowserContext* browser_context,
                            const Extension* extension,
                            base::Optional<int> tab_id,
                            std::string* error) {
  const PermissionsData* permissions_data = extension->permissions_data();

  const auto kFeedbackPermission =
      APIPermission::kDeclarativeNetRequestFeedback;

  bool can_call = tab_id.has_value()
                      ? permissions_data->HasAPIPermissionForTab(
                            *tab_id, kFeedbackPermission)
                      : permissions_data->HasAPIPermission(kFeedbackPermission);

  if (!can_call)
    *error = declarative_net_request::kErrorGetMatchedRulesMissingPermissions;

  return can_call;
}

}  // namespace

DeclarativeNetRequestUpdateDynamicRulesFunction::
    DeclarativeNetRequestUpdateDynamicRulesFunction() = default;
DeclarativeNetRequestUpdateDynamicRulesFunction::
    ~DeclarativeNetRequestUpdateDynamicRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestUpdateDynamicRulesFunction::Run() {
  using Params = dnr_api::UpdateDynamicRules::Params;

  base::string16 error;
  std::unique_ptr<Params> params(Params::Create(*args_, &error));
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
    base::Optional<std::string> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error)
    Respond(Error(*error));
  else
    Respond(NoArguments());
}

DeclarativeNetRequestGetDynamicRulesFunction::
    DeclarativeNetRequestGetDynamicRulesFunction() = default;
DeclarativeNetRequestGetDynamicRulesFunction::
    ~DeclarativeNetRequestGetDynamicRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetDynamicRulesFunction::Run() {
  auto source = declarative_net_request::RulesetSource::CreateDynamic(
      browser_context(), extension()->id());

  auto read_dynamic_rules = base::BindOnce(
      [](const declarative_net_request::RulesetSource& source) {
        return source.ReadJSONRulesUnsafe();
      },
      std::move(source));

  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      std::move(read_dynamic_rules),
      base::BindOnce(
          &DeclarativeNetRequestGetDynamicRulesFunction::OnDynamicRulesFetched,
          this));
  return RespondLater();
}

void DeclarativeNetRequestGetDynamicRulesFunction::OnDynamicRulesFetched(
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

  Respond(ArgumentList(
      dnr_api::GetDynamicRules::Results::Create(read_json_result.rules)));
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

  base::string16 error;
  std::unique_ptr<Params> params(Params::Create(*args_, &error));
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
    OnEnabledStaticRulesetsUpdated(base::Optional<std::string> error) {
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

    // Exclude the dynamic ruleset ID if present, as expected by the API
    // function.
    base::Erase(public_ids, dnr_api::DYNAMIC_RULESET_ID);
  }

  return RespondNow(
      ArgumentList(dnr_api::GetEnabledRulesets::Results::Create(public_ids)));
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

  base::string16 error;
  std::unique_ptr<Params> params(Params::Create(*args_, &error));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  base::Optional<int> tab_id;
  base::Time min_time_stamp = base::Time::Min();

  if (params->filter) {
    if (params->filter->tab_id)
      tab_id = *params->filter->tab_id;

    if (params->filter->min_time_stamp)
      min_time_stamp = base::Time::FromJsTime(*params->filter->min_time_stamp);
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
      base::TimeDelta::FromMinutes(dnr_api::GETMATCHEDRULES_QUOTA_INTERVAL)};

  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      limit, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL"));
}

bool DeclarativeNetRequestGetMatchedRulesFunction::ShouldSkipQuotaLimiting()
    const {
  return user_gesture() || disable_throttling_for_test_;
}

DeclarativeNetRequestSetActionCountAsBadgeTextFunction::
    DeclarativeNetRequestSetActionCountAsBadgeTextFunction() = default;
DeclarativeNetRequestSetActionCountAsBadgeTextFunction::
    ~DeclarativeNetRequestSetActionCountAsBadgeTextFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestSetActionCountAsBadgeTextFunction::Run() {
  using Params = dnr_api::SetActionCountAsBadgeText::Params;

  base::string16 error;
  std::unique_ptr<Params> params(Params::Create(*args_, &error));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  if (params->enable == prefs->GetDNRUseActionCountAsBadgeText(extension_id()))
    return RespondNow(NoArguments());

  prefs->SetDNRUseActionCountAsBadgeText(extension_id(), params->enable);

  // If the preference is switched on, update the extension's badge text with
  // the number of actions matched for this extension. Otherwise, clear the
  // action count for the extension's icon and show the default badge text if
  // set.
  if (params->enable) {
    declarative_net_request::RulesMonitorService* rules_monitor_service =
        declarative_net_request::RulesMonitorService::Get(browser_context());
    DCHECK(rules_monitor_service);

    const declarative_net_request::ActionTracker& action_tracker =
        rules_monitor_service->action_tracker();
    action_tracker.OnPreferenceEnabled(extension_id());
  } else {
    DCHECK(ExtensionsAPIClient::Get());
    ExtensionsAPIClient::Get()->ClearActionCount(browser_context(),
                                                 *extension());
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

  base::string16 error;
  std::unique_ptr<Params> params(Params::Create(*args_, &error));
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
                        ? dnr_api::UNSUPPORTED_REGEX_REASON_MEMORYLIMITEXCEEDED
                        : dnr_api::UNSUPPORTED_REGEX_REASON_SYNTAXERROR;
  }

  return RespondNow(
      ArgumentList(dnr_api::IsRegexSupported::Results::Create(result)));
}

}  // namespace extensions

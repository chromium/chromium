// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
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
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

namespace {

namespace dnr_api = api::declarative_net_request;

// Returns true if the given |extension| has a registered ruleset. If it
// doesn't, returns false and populates |error|.
// TODO(crbug.com/931967): Using HasRegisteredRuleset for PreRunValidation means
// that the extension function will fail if the ruleset for the extension is
// currently being indexed. Fix this.
bool HasRegisteredRuleset(content::BrowserContext* context,
                          const ExtensionId& extension_id,
                          std::string* error) {
  const auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(context);
  DCHECK(rules_monitor_service);

  if (rules_monitor_service->HasRegisteredRuleset(extension_id))
    return true;

  *error = "The extension must have a ruleset in order to call this function.";
  return false;
}

}  // namespace

DeclarativeNetRequestUpdateAllowedPagesFunction::
    DeclarativeNetRequestUpdateAllowedPagesFunction() = default;
DeclarativeNetRequestUpdateAllowedPagesFunction::
    ~DeclarativeNetRequestUpdateAllowedPagesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestUpdateAllowedPagesFunction::UpdateAllowedPages(
    const std::vector<std::string>& patterns,
    Action action) {
  if (patterns.empty())
    return RespondNow(NoArguments());

  // It's ok to allow file access and to use SCHEME_ALL since this is not
  // actually granting any permissions to the extension. This will only be used
  // to allow requests.
  URLPatternSet delta;
  std::string error;
  if (!delta.Populate(patterns, URLPattern::SCHEME_ALL,
                      true /*allow_file_access*/, &error)) {
    return RespondNow(Error(error));
  }

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  URLPatternSet current_set = prefs->GetDNRAllowedPages(extension_id());
  URLPatternSet new_set;
  switch (action) {
    case Action::ADD:
      new_set = URLPatternSet::CreateUnion(current_set, delta);
      break;
    case Action::REMOVE:
      new_set = URLPatternSet::CreateDifference(current_set, delta);
      break;
  }

  if (static_cast<int>(new_set.size()) > dnr_api::MAX_NUMBER_OF_ALLOWED_PAGES) {
    return RespondNow(Error(base::StringPrintf(
        "The number of allowed page patterns can't exceed %d",
        dnr_api::MAX_NUMBER_OF_ALLOWED_PAGES)));
  }

  // Persist |new_set| as part of preferences.
  prefs->SetDNRAllowedPages(extension_id(), new_set.Clone());

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  rules_monitor_service->ruleset_manager()->UpdateAllowedPages(
      extension_id(), std::move(new_set));

  return RespondNow(NoArguments());
}

bool DeclarativeNetRequestUpdateAllowedPagesFunction::PreRunValidation(
    std::string* error) {
  return ExtensionFunction::PreRunValidation(error) &&
         HasRegisteredRuleset(browser_context(), extension_id(), error);
}

DeclarativeNetRequestAddAllowedPagesFunction::
    DeclarativeNetRequestAddAllowedPagesFunction() = default;
DeclarativeNetRequestAddAllowedPagesFunction::
    ~DeclarativeNetRequestAddAllowedPagesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestAddAllowedPagesFunction::Run() {
  using Params = dnr_api::AddAllowedPages::Params;

  base::string16 error;
  std::unique_ptr<Params> params(Params::Create(*args_, &error));
  EXTENSION_FUNCTION_VALIDATE(params);

  // EXTENSION_FUNCTION_VALIDATE should validate that the arguments are in the
  // correct format. Ignore |error|.

  return UpdateAllowedPages(params->page_patterns, Action::ADD);
}

DeclarativeNetRequestRemoveAllowedPagesFunction::
    DeclarativeNetRequestRemoveAllowedPagesFunction() = default;
DeclarativeNetRequestRemoveAllowedPagesFunction::
    ~DeclarativeNetRequestRemoveAllowedPagesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestRemoveAllowedPagesFunction::Run() {
  using Params = dnr_api::AddAllowedPages::Params;

  base::string16 error;
  std::unique_ptr<Params> params(Params::Create(*args_, &error));
  EXTENSION_FUNCTION_VALIDATE(params);

  // EXTENSION_FUNCTION_VALIDATE should validate that the arguments are in the
  // correct format. Ignore |error|.

  return UpdateAllowedPages(params->page_patterns, Action::REMOVE);
}

DeclarativeNetRequestGetAllowedPagesFunction::
    DeclarativeNetRequestGetAllowedPagesFunction() = default;
DeclarativeNetRequestGetAllowedPagesFunction::
    ~DeclarativeNetRequestGetAllowedPagesFunction() = default;

bool DeclarativeNetRequestGetAllowedPagesFunction::PreRunValidation(
    std::string* error) {
  return ExtensionFunction::PreRunValidation(error) &&
         HasRegisteredRuleset(browser_context(), extension_id(), error);
}

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetAllowedPagesFunction::Run() {
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  URLPatternSet current_set = prefs->GetDNRAllowedPages(extension_id());

  return RespondNow(ArgumentList(dnr_api::GetAllowedPages::Results::Create(
      *current_set.ToStringVector())));
}

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

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(browser_context());
  DCHECK(rules_monitor_service);
  DCHECK(extension());

  auto callback = base::BindOnce(
      &DeclarativeNetRequestUpdateDynamicRulesFunction::OnDynamicRulesUpdated,
      this);

  rules_monitor_service->UpdateDynamicRules(
      *extension(), std::move(params->rule_ids_to_remove),
      std::move(params->rules_to_add), std::move(callback));
  return RespondLater();
}

bool DeclarativeNetRequestUpdateDynamicRulesFunction::PreRunValidation(
    std::string* error) {
  return ExtensionFunction::PreRunValidation(error) &&
         HasRegisteredRuleset(browser_context(), extension_id(), error);
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

bool DeclarativeNetRequestGetDynamicRulesFunction::PreRunValidation(
    std::string* error) {
  return ExtensionFunction::PreRunValidation(error) &&
         HasRegisteredRuleset(browser_context(), extension_id(), error);
}

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetDynamicRulesFunction::Run() {
  auto source = declarative_net_request::RulesetSource::CreateDynamic(
      browser_context(), *extension());

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

DeclarativeNetRequestGetMatchedRulesFunction::
    DeclarativeNetRequestGetMatchedRulesFunction() = default;
DeclarativeNetRequestGetMatchedRulesFunction::
    ~DeclarativeNetRequestGetMatchedRulesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetMatchedRulesFunction::Run() {
  return RespondNow(NoArguments());
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

}  // namespace extensions

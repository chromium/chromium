// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

namespace {

// Returns true if the given |extension| has a registered ruleset. If it
// doesn't, returns false and populates |error|.
bool HasRegisteredRuleset(content::BrowserContext* context,
                          const ExtensionId& extension_id,
                          std::string* error) {
  const auto* rules_monitor_service = BrowserContextKeyedAPIFactory<
      declarative_net_request::RulesMonitorService>::Get(context);
  DCHECK(rules_monitor_service);

  if (rules_monitor_service->HasRegisteredRuleset(extension_id))
    return true;

  *error = "The extension must have a ruleset in order to call this function.";
  return false;
}

void UpdateAllowPagesOnIOThread(const ExtensionId& extension_id,
                                URLPatternSet allowed_pages,
                                InfoMap* info_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);

  info_map->GetRulesetManager()->UpdateAllowedPages(extension_id,
                                                    std::move(allowed_pages));
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

  if (static_cast<int>(new_set.size()) >
      api::declarative_net_request::MAX_NUMBER_OF_ALLOWED_PAGES) {
    return RespondNow(Error(base::StringPrintf(
        "The number of allowed page patterns can't exceed %d",
        api::declarative_net_request::MAX_NUMBER_OF_ALLOWED_PAGES)));
  }

  // Persist |new_set| as part of preferences.
  prefs->SetDNRAllowedPages(extension_id(), new_set);

  // Update the new allowed set on the IO thread.
  base::OnceClosure updated_allow_pages_io_task = base::BindOnce(
      &UpdateAllowPagesOnIOThread, extension_id(), std::move(new_set),
      base::RetainedRef(ExtensionSystem::Get(browser_context())->info_map()));

  base::OnceClosure updated_allowed_pages_ui_reply = base::BindOnce(
      &DeclarativeNetRequestUpdateAllowedPagesFunction::OnAllowedPagesUpdated,
      this);
  base::PostTaskWithTraitsAndReply(FROM_HERE, {content::BrowserThread::IO},
                                   std::move(updated_allow_pages_io_task),
                                   std::move(updated_allowed_pages_ui_reply));

  return RespondLater();
}

void DeclarativeNetRequestUpdateAllowedPagesFunction::OnAllowedPagesUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Respond(NoArguments());
}

bool DeclarativeNetRequestUpdateAllowedPagesFunction::PreRunValidation(
    std::string* error) {
  return UIThreadExtensionFunction::PreRunValidation(error) &&
         HasRegisteredRuleset(browser_context(), extension_id(), error);
}

DeclarativeNetRequestAddAllowedPagesFunction::
    DeclarativeNetRequestAddAllowedPagesFunction() = default;
DeclarativeNetRequestAddAllowedPagesFunction::
    ~DeclarativeNetRequestAddAllowedPagesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestAddAllowedPagesFunction::Run() {
  using Params = api::declarative_net_request::AddAllowedPages::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return UpdateAllowedPages(params->page_patterns, Action::ADD);
}

DeclarativeNetRequestRemoveAllowedPagesFunction::
    DeclarativeNetRequestRemoveAllowedPagesFunction() = default;
DeclarativeNetRequestRemoveAllowedPagesFunction::
    ~DeclarativeNetRequestRemoveAllowedPagesFunction() = default;

ExtensionFunction::ResponseAction
DeclarativeNetRequestRemoveAllowedPagesFunction::Run() {
  using Params = api::declarative_net_request::AddAllowedPages::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return UpdateAllowedPages(params->page_patterns, Action::REMOVE);
}

DeclarativeNetRequestGetAllowedPagesFunction::
    DeclarativeNetRequestGetAllowedPagesFunction() = default;
DeclarativeNetRequestGetAllowedPagesFunction::
    ~DeclarativeNetRequestGetAllowedPagesFunction() = default;

bool DeclarativeNetRequestGetAllowedPagesFunction::PreRunValidation(
    std::string* error) {
  return UIThreadExtensionFunction::PreRunValidation(error) &&
         HasRegisteredRuleset(browser_context(), extension_id(), error);
}

ExtensionFunction::ResponseAction
DeclarativeNetRequestGetAllowedPagesFunction::Run() {
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  URLPatternSet current_set = prefs->GetDNRAllowedPages(extension_id());

  return RespondNow(ArgumentList(
      api::declarative_net_request::GetAllowedPages::Results::Create(
          *current_set.ToStringVector())));
}

}  // namespace extensions

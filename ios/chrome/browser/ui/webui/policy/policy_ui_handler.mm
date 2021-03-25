// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/policy/policy_ui_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/browser_state_policy_connector.h"
#include "ios/chrome/browser/policy/policy_conversions_client_ios.h"
#include "ui/base/webui/web_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PolicyUIHandler::PolicyUIHandler() {}

PolicyUIHandler::~PolicyUIHandler() {
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy::SchemaRegistry* registry = ChromeBrowserState::FromWebUIIOS(web_ui())
                                         ->GetPolicyConnector()
                                         ->GetSchemaRegistry();
  registry->RemoveObserver(this);
}

void PolicyUIHandler::AddCommonLocalizedStringsToSource(
    web::WebUIIOSDataSource* source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"conflict", IDS_POLICY_LABEL_CONFLICT},
      {"superseding", IDS_POLICY_LABEL_SUPERSEDING},
      {"conflictValue", IDS_POLICY_LABEL_CONFLICT_VALUE},
      {"supersededValue", IDS_POLICY_LABEL_SUPERSEDED_VALUE},
      {"headerLevel", IDS_POLICY_HEADER_LEVEL},
      {"headerName", IDS_POLICY_HEADER_NAME},
      {"headerScope", IDS_POLICY_HEADER_SCOPE},
      {"headerSource", IDS_POLICY_HEADER_SOURCE},
      {"headerStatus", IDS_POLICY_HEADER_STATUS},
      {"headerValue", IDS_POLICY_HEADER_VALUE},
      {"warning", IDS_POLICY_HEADER_WARNING},
      {"levelMandatory", IDS_POLICY_LEVEL_MANDATORY},
      {"levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED},
      {"error", IDS_POLICY_LABEL_ERROR},
      {"deprecated", IDS_POLICY_LABEL_DEPRECATED},
      {"future", IDS_POLICY_LABEL_FUTURE},
      {"info", IDS_POLICY_LABEL_INFO},
      {"ignored", IDS_POLICY_LABEL_IGNORED},
      {"notSpecified", IDS_POLICY_NOT_SPECIFIED},
      {"ok", IDS_POLICY_OK},
      {"scopeDevice", IDS_POLICY_SCOPE_DEVICE},
      {"scopeUser", IDS_POLICY_SCOPE_USER},
      {"title", IDS_POLICY_TITLE},
      {"unknown", IDS_POLICY_UNKNOWN},
      {"unset", IDS_POLICY_UNSET},
      {"value", IDS_POLICY_LABEL_VALUE},
      {"sourceDefault", IDS_POLICY_SOURCE_DEFAULT},
      {"loadPoliciesDone", IDS_POLICY_LOAD_POLICIES_DONE},
      {"loadingPolicies", IDS_POLICY_LOADING_POLICIES},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddLocalizedStrings(policy::kPolicySources);
  source->UseStringsJs();
}

void PolicyUIHandler::RegisterMessages() {
  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  browser_state->GetPolicyConnector()->GetSchemaRegistry()->AddObserver(this);

  web_ui()->RegisterMessageCallback(
      "listenPoliciesUpdates",
      base::BindRepeating(&PolicyUIHandler::HandleListenPoliciesUpdates,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reloadPolicies",
      base::BindRepeating(&PolicyUIHandler::HandleReloadPolicies,
                          base::Unretained(this)));
}

void PolicyUIHandler::OnSchemaRegistryUpdated(bool has_new_schemas) {
  // Update UI when new schema is added.
  if (has_new_schemas) {
    SendPolicies();
  }
}

void PolicyUIHandler::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  SendPolicies();
}

base::Value PolicyUIHandler::GetPolicyNames() const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  policy::SchemaRegistry* registry =
      browser_state->GetPolicyConnector()->GetSchemaRegistry();
  scoped_refptr<policy::SchemaMap> schema_map = registry->schema_map();

  // Add Chrome policy names.
  base::Value chrome_policy_names(base::Value::Type::LIST);
  policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME, "");
  const policy::Schema* chrome_schema = schema_map->GetSchema(chrome_namespace);
  for (auto it = chrome_schema->GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    chrome_policy_names.Append(base::Value(it.key()));
  }

  base::Value chrome_values(base::Value::Type::DICTIONARY);
  chrome_values.SetStringKey("name", "Chrome Policies");
  chrome_values.SetKey("policyNames", std::move(chrome_policy_names));

  base::Value names(base::Value::Type::DICTIONARY);
  names.SetKey("chrome", std::move(chrome_values));
  return names;
}

base::Value PolicyUIHandler::GetPolicyValues() const {
  auto client = std::make_unique<PolicyConversionsClientIOS>(
      ChromeBrowserState::FromWebUIIOS(web_ui()));
  return policy::ArrayPolicyConversions(std::move(client))
      .EnableConvertValues(true)
      .ToValue();
}

void PolicyUIHandler::HandleListenPoliciesUpdates(const base::ListValue* args) {
  OnRefreshPoliciesDone();
}

void PolicyUIHandler::HandleReloadPolicies(const base::ListValue* args) {
  GetPolicyService()->RefreshPolicies(base::BindOnce(
      &PolicyUIHandler::OnRefreshPoliciesDone, weak_factory_.GetWeakPtr()));
}

void PolicyUIHandler::SendPolicies() {
  base::Value names = GetPolicyNames();
  base::Value values = GetPolicyValues();
  std::vector<const base::Value*> args;
  args.push_back(&names);
  args.push_back(&values);
  web_ui()->FireWebUIListener("policies-updated", args);
}

void PolicyUIHandler::OnRefreshPoliciesDone() {
  SendPolicies();
}

policy::PolicyService* PolicyUIHandler::GetPolicyService() const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  return browser_state->GetPolicyConnector()->GetPolicyService();
}

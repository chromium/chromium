// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema_registry.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace policy {
class PolicyMap;
struct PolicyNamespace;
}  // namespace policy

// The JavaScript message handler for the chrome://policy page.
class PolicyUIHandler : public web::WebUIIOSMessageHandler,
                        public policy::PolicyService::Observer,
                        public policy::PolicyStatusProvider::Observer,
                        public policy::SchemaRegistry::Observer {
 public:
  PolicyUIHandler();
  ~PolicyUIHandler() override;
  PolicyUIHandler(const PolicyUIHandler&) = delete;
  PolicyUIHandler& operator=(const PolicyUIHandler&) = delete;

  static void AddCommonLocalizedStringsToSource(
      web::WebUIIOSDataSource* source);

  // web::WebUIIOSMessageHandler.
  void RegisterMessages() override;

  // policy::PolicyService::Observer.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // policy::PolicyValueProvider::Observer implementation.
  void OnPolicyStatusChanged() override;

  // Called when report has been uploaded, successfully or not.
  void OnReportUploaded(const std::string& callback_id);

  // policy::SchemaRegistry::Observer.
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

 private:
  // Returns a dictionary containing the policies supported by Chrome.
  base::Value::Dict GetPolicyNames() const;

  // Returns a dictionary containing the current values of the policies
  // supported by Chrome and list of the policy IDs.
  base::Value::Dict GetPolicyValues() const;

  // Called to handle the "listenPoliciesUpdates" WebUI message.
  void HandleListenPoliciesUpdates(const base::Value::List& args);

  // Called to handle the "copyPoliciesJSON" WebUI message.
  void HandleCopyPoliciesJson(const base::Value::List& args);

  // Called to handle the "reloadPolicies" WebUI message.
  void HandleReloadPolicies(const base::Value::List& args);

  // Called to handle the "uploadReport" WebUI message.
  void HandleUploadReport(const base::Value::List& args);

  // Send information about the current policy values to the UI. For each policy
  // whose value has been set, dictionaries containing the value and additional
  // metadata are sent.
  void SendPolicies();

  // Get a value dictionary of cloud policies' status information for each scope
  // that has cloud policy enabled (device and/or user).
  base::Value::Dict GetStatusValue() const;

  void SendStatus();

  // The callback invoked by PolicyService::RefreshPolicies().
  void OnRefreshPoliciesDone();

  // Build a JSON string of all the policies.
  std::string GetPoliciesAsJson();

  // Returns the PolicyService associated with this WebUI's BrowserState.
  policy::PolicyService* GetPolicyService() const;

  // Provider that supply status dictionary for machine policy,
  std::unique_ptr<policy::PolicyStatusProvider> machine_status_provider_;

  base::ScopedObservation<policy::PolicyStatusProvider,
                          policy::PolicyStatusProvider::Observer>
      machine_status_provider_observation_{this};

  // Vends WeakPtrs for this object.
  base::WeakPtrFactory<PolicyUIHandler> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_

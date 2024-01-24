// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_STATE_POLICY_CONNECTOR_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_STATE_POLICY_CONNECTOR_H_

#include <memory>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "components/policy/core/common/local_test_policy_provider.h"

class BrowserPolicyConnectorIOS;

namespace policy {
class ConfigurationPolicyProvider;
class PolicyService;
class SchemaRegistry;
}  // namespace policy

// BrowserStatePolicyConnector creates and manages the per-BrowserState policy
// components and their integration with PrefService.
// BrowserStatePolicyConnector isn't a keyed service because the pref service,
// which isn't a keyed service, has a hard dependency on the policy
// infrastructure. In order to outlive the pref service, the policy connector
// must live outside the keyed services.
class BrowserStatePolicyConnector {
 public:
  BrowserStatePolicyConnector();
  ~BrowserStatePolicyConnector();
  BrowserStatePolicyConnector(const BrowserStatePolicyConnector&) = delete;
  BrowserStatePolicyConnector& operator=(const BrowserStatePolicyConnector&) =
      delete;

  // Initializes this connector.
  void Init(policy::SchemaRegistry* schema_registry,
            BrowserPolicyConnectorIOS* browser_policy_connector,
            policy::ConfigurationPolicyProvider* user_policy_provider);

  // Sets the local_test_policy_provider as active and all other policy
  // providers to inactive.
  void UseLocalTestPolicyProvider();

  // Reverts the effects of UseLocalTestPolicyProvider.
  void RevertUseLocalTestPolicyProvider();

  // Shuts this connector down in preparation for destruction.
  void Shutdown();

  // Returns the PolicyService managed by this connector.  This is never
  // nullptr.
  policy::PolicyService* GetPolicyService() const {
    return policy_service_.get();
  }

  // Returns the SchemaRegistry associated with this connector.
  policy::SchemaRegistry* GetSchemaRegistry() const { return schema_registry_; }

 private:
  friend class BrowserStatePolicyConnectorMock;

  // `policy_providers_` contains a list of the policy providers available for
  // the PolicyService of this connector, in decreasing order of priority.
  //
  // Note: All the providers appended to this vector must eventually become
  // initialized for every policy domain, otherwise some subsystems will never
  // use the policies exposed by the PolicyService!
  // The default ConfigurationPolicyProvider::IsInitializationComplete()
  // result is true, so take care if a provider overrides that.
  std::vector<raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>
      policy_providers_;

  raw_ptr<policy::LocalTestPolicyProvider> local_test_policy_provider_ =
      nullptr;

  // The PolicyService that manages policy for this connector's BrowserState.
  std::unique_ptr<policy::PolicyService> policy_service_;

  // The SchemaRegistry associated with this connector's BrowserState.
  raw_ptr<policy::SchemaRegistry> schema_registry_;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_STATE_POLICY_CONNECTOR_H_

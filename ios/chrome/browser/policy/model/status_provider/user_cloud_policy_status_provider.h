// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_H_

#import "base/containers/flat_set.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/policy/core/browser/webui/policy_status_provider.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"

namespace signin {
class IdentityManager;
}

// A status provider for user policy.
class UserCloudPolicyStatusProvider
    : public policy::PolicyStatusProvider,
      public policy::CloudPolicyStore::Observer,
      public policy::CloudPolicyCore::Observer,
      public policy::CloudPolicyClient::Observer {
 public:
  // Delegate to give data to the provider.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Gets the affiliation IDs for the device.
    virtual base::flat_set<std::string> GetDeviceAffiliationIds() = 0;
  };

  UserCloudPolicyStatusProvider(Delegate* delegate,
                                policy::CloudPolicyCore* user_level_policy_core,
                                signin::IdentityManager* identity_manager);

  UserCloudPolicyStatusProvider(const UserCloudPolicyStatusProvider&) = delete;
  UserCloudPolicyStatusProvider& operator=(
      const UserCloudPolicyStatusProvider&) = delete;

  ~UserCloudPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  // policy::CloudPolicyCore::Observer implementation.
  void OnCoreConnected(policy::CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) override;
  void OnCoreDisconnecting(policy::CloudPolicyCore* core) override;

  // policy::CloudPolicyClient::Observer implementation
  void OnPolicyFetched(policy::CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(policy::CloudPolicyClient* client) override;
  void OnClientError(policy::CloudPolicyClient* client) override;

 private:
  // Returns true if the user and machine level policies are affiliated.
  bool IsAffiliated();

  raw_ptr<Delegate> delegate_;
  raw_ptr<policy::CloudPolicyCore> user_level_policy_core_;
  raw_ptr<const signin::IdentityManager> identity_manager_;

  base::ScopedObservation<policy::CloudPolicyCore,
                          policy::CloudPolicyCore::Observer>
      core_observation_{this};
  base::ScopedObservation<policy::CloudPolicyClient,
                          policy::CloudPolicyClient::Observer>
      client_observation_{this};
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      store_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_STATUS_PROVIDER_USER_CLOUD_POLICY_STATUS_PROVIDER_H_

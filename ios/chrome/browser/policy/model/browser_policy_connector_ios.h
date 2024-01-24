// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_POLICY_CONNECTOR_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_POLICY_CONNECTOR_IOS_H_

#import <memory>
#import <string>

#import "base/containers/flat_set.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/common/local_test_policy_provider.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {
class ConfigurationPolicyProvider;
class ChromeBrowserCloudManagementController;
class MachineLevelUserCloudPolicyManager;
}  // namespace policy

// Extends BrowserPolicyConnector with the setup for iOS builds.
class BrowserPolicyConnectorIOS : public policy::BrowserPolicyConnector {
 public:
  // Service initialization delay time in millisecond on startup. (So that
  // displaying Chrome's GUI does not get delayed.)
  static const int64_t kServiceInitializationStartupDelay = 5000;

  BrowserPolicyConnectorIOS(
      const policy::HandlerListFactory& handler_list_factory);
  BrowserPolicyConnectorIOS(const BrowserPolicyConnectorIOS&) = delete;
  BrowserPolicyConnectorIOS& operator=(const BrowserPolicyConnectorIOS&) =
      delete;

  ~BrowserPolicyConnectorIOS() override;

  // If the kLocalTestPoliciesForNextStartup pref is non-empty, read and apply
  // the policies stored in it, and then clear the pref.
  void MaybeApplyLocalTestPolicies(PrefService* local_state);

  // Returns the platform provider used by this BrowserPolicyConnectorIOS. Can
  // be overridden for testing via
  // BrowserPolicyConnectorBase::SetPolicyProviderForTesting().
  policy::ConfigurationPolicyProvider* GetPlatformProvider();

  // Returns the device affiliations IDs associated with the browser.
  base::flat_set<std::string> GetDeviceAffiliationIds();

  policy::LocalTestPolicyProvider* local_test_policy_provider() {
    return local_test_provider_.get();
  }

  policy::ChromeBrowserCloudManagementController*
  chrome_browser_cloud_management_controller() {
    return chrome_browser_cloud_management_controller_.get();
  }

  policy::MachineLevelUserCloudPolicyManager*
  machine_level_user_cloud_policy_manager() {
    return machine_level_user_cloud_policy_manager_;
  }

  // BrowserPolicyConnector.
  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;
  bool IsDeviceEnterpriseManaged() const override;
  bool HasMachineLevelPolicies() override;
  void Shutdown() override;

  // BrowserPolicyConnector.
  // Always returns true because there is no way for normal users to use command
  // line switch anyway.
  bool IsCommandLineSwitchSupported() const override;

 protected:
  // BrowserPolicyConnectorBase.
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

 private:
  std::unique_ptr<policy::ConfigurationPolicyProvider> CreatePlatformProvider();

  // Owned by base class.
  raw_ptr<policy::LocalTestPolicyProvider> local_test_provider_ = nullptr;
  raw_ptr<policy::ConfigurationPolicyProvider> platform_provider_ = nullptr;

  std::unique_ptr<policy::ChromeBrowserCloudManagementController>
      chrome_browser_cloud_management_controller_;
  raw_ptr<policy::MachineLevelUserCloudPolicyManager>
      machine_level_user_cloud_policy_manager_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_POLICY_CONNECTOR_IOS_H_

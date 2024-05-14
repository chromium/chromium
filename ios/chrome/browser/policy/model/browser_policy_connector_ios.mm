// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"

#import <stdint.h>

#import <utility>

#import "base/functional/callback.h"
#import "base/system/sys_info.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#import "components/policy/core/common/async_policy_provider.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/core/common/cloud/device_management_service.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/configuration_policy_provider.h"
#import "components/policy/core/common/local_test_policy_provider.h"
#import "components/policy/core/common/policy_loader_ios.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_ios.h"
#import "ios/chrome/browser/policy/model/device_management_service_configuration_ios.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using policy::AsyncPolicyLoader;
using policy::AsyncPolicyProvider;
using policy::BrowserPolicyConnector;
using policy::BrowserPolicyConnectorBase;
using policy::ConfigurationPolicyProvider;
using policy::HandlerListFactory;
using policy::LocalTestPolicyProvider;
using policy::PolicyLoaderIOS;

BrowserPolicyConnectorIOS::BrowserPolicyConnectorIOS(
    const HandlerListFactory& handler_list_factory)
    : BrowserPolicyConnector(handler_list_factory) {
  chrome_browser_cloud_management_controller_ = std::make_unique<
      policy::ChromeBrowserCloudManagementController>(
      std::make_unique<policy::ChromeBrowserCloudManagementControllerIOS>());
}

BrowserPolicyConnectorIOS::~BrowserPolicyConnectorIOS() {}

ConfigurationPolicyProvider* BrowserPolicyConnectorIOS::GetPlatformProvider() {
  ConfigurationPolicyProvider* provider =
      BrowserPolicyConnectorBase::GetPolicyProviderForTesting();
  return provider ? provider : platform_provider_.get();
}

base::flat_set<std::string>
BrowserPolicyConnectorIOS::GetDeviceAffiliationIds() {
  if (!machine_level_user_cloud_policy_manager_ ||
      !policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid()) {
    return {};
  }

  const auto* core = machine_level_user_cloud_policy_manager_->core();
  CHECK(core);

  return policy::GetAffiliationIdsFromCore(*core, /*for_device=*/true);
}

void BrowserPolicyConnectorIOS::MaybeApplyLocalTestPolicies(
    PrefService* local_state) {
  std::string policies_to_apply = local_state->GetString(
      policy::policy_prefs::kLocalTestPoliciesForNextStartup);
  if (policies_to_apply.empty()) {
    return;
  }
  for (ConfigurationPolicyProvider* provider : GetPolicyProviders()) {
    provider->set_active(false);
  }
  local_test_provider_->set_active(true);
  local_test_provider_->LoadJsonPolicies(policies_to_apply);
  local_state->ClearPref(
      policy::policy_prefs::kLocalTestPoliciesForNextStartup);
}

void BrowserPolicyConnectorIOS::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  policy::PolicyLogger::GetInstance()->EnableLogDeletion();
  std::unique_ptr<policy::DeviceManagementService::Configuration> configuration(
      new policy::DeviceManagementServiceConfigurationIOS(
          GetDeviceManagementUrl(), GetRealtimeReportingUrl(),
          GetEncryptedReportingUrl()));
  std::unique_ptr<policy::DeviceManagementService> device_management_service(
      new policy::DeviceManagementService(std::move(configuration)));
  device_management_service->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  InitInternal(local_state, std::move(device_management_service));
  MaybeApplyLocalTestPolicies(local_state);
}

bool BrowserPolicyConnectorIOS::IsDeviceEnterpriseManaged() const {
  NOTREACHED_IN_MIGRATION() << "This method is only defined for Chrome OS";
  return false;
}

bool BrowserPolicyConnectorIOS::HasMachineLevelPolicies() {
  return ProviderHasPolicies(GetPlatformProvider()) ||
         ProviderHasPolicies(machine_level_user_cloud_policy_manager_);
}

void BrowserPolicyConnectorIOS::Shutdown() {
  // Reset the controller before calling base class so that
  // shutdown occurs in correct sequence.
  chrome_browser_cloud_management_controller_.reset();

  BrowserPolicyConnector::Shutdown();
}

bool BrowserPolicyConnectorIOS::IsCommandLineSwitchSupported() const {
  return true;
}

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
BrowserPolicyConnectorIOS::CreatePolicyProviders() {
  auto providers = BrowserPolicyConnector::CreatePolicyProviders();
  std::unique_ptr<ConfigurationPolicyProvider> platform_provider =
      CreatePlatformProvider();
  if (platform_provider) {
    DCHECK(!platform_provider_) << "CreatePolicyProviders was called twice.";
    platform_provider_ = platform_provider.get();
    // PlatformProvider should be before all other providers (highest priority).
    providers.insert(providers.begin(), std::move(platform_provider));
  }

  std::unique_ptr<policy::MachineLevelUserCloudPolicyManager>
      machine_level_user_cloud_policy_manager =
          chrome_browser_cloud_management_controller_->CreatePolicyManager(
              platform_provider_);
  if (machine_level_user_cloud_policy_manager) {
    machine_level_user_cloud_policy_manager_ =
        machine_level_user_cloud_policy_manager.get();
    providers.push_back(std::move(machine_level_user_cloud_policy_manager));
  }

  std::unique_ptr<LocalTestPolicyProvider> local_test_provider =
      LocalTestPolicyProvider::CreateIfAllowed(GetChannel());

  if (local_test_provider) {
    local_test_provider_ = local_test_provider.get();
    providers.push_back(std::move(local_test_provider));
  }

  return providers;
}

std::unique_ptr<ConfigurationPolicyProvider>
BrowserPolicyConnectorIOS::CreatePlatformProvider() {
  auto loader = std::make_unique<PolicyLoaderIOS>(
      GetSchemaRegistry(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));

  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
}

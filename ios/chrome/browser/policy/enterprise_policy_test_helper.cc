// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/enterprise_policy_test_helper.h"

#include "base/task/single_thread_task_runner.h"
#include "components/policy/core/common/policy_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/policy/browser_state_policy_connector.h"
#include "ios/chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

EnterprisePolicyTestHelper::EnterprisePolicyTestHelper(
    const base::FilePath& state_directory_path) {
  policy_provider_.SetDefaultReturns(
      true /* is_initialization_complete_return */,
      true /* is_first_policy_load_complete_return */);
  // Create a BrowserPolicyConnectorIOS, install the mock policy
  // provider, and hook up Local State.
  browser_policy_connector_ = std::make_unique<BrowserPolicyConnectorIOS>(
      base::BindRepeating(&BuildPolicyHandlerList,
                          /* are_future_policies_allowed_by_default= */ true));
  browser_policy_connector_->SetPolicyProviderForTesting(&policy_provider_);

  scoped_refptr<PrefRegistrySimple> local_state_registry(
      new PrefRegistrySimple);
  RegisterLocalStatePrefs(local_state_registry.get());
  local_state_ = CreateLocalState(
      state_directory_path.Append("TestLocalState"),
      base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      local_state_registry, browser_policy_connector_->GetPolicyService(),
      browser_policy_connector_.get());
  browser_policy_connector_->Init(local_state_.get(), nullptr);

  // Create a BrowserStatePolicyConnector and hook it up to prefs.
  browser_state_policy_connector_ =
      std::make_unique<BrowserStatePolicyConnector>();
  browser_state_policy_connector_->Init(
      browser_policy_connector_->GetSchemaRegistry(),
      browser_policy_connector_.get(), /*user_policy_provider=*/nullptr);
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry(
      new user_prefs::PrefRegistrySyncable);
  RegisterBrowserStatePrefs(pref_registry.get());
  std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service =
      CreateBrowserStatePrefs(
          state_directory_path,
          base::SingleThreadTaskRunner::GetCurrentDefault().get(),
          pref_registry, browser_state_policy_connector_->GetPolicyService(),
          browser_policy_connector_.get());

  TestChromeBrowserState::Builder builder;
  builder.SetPath(state_directory_path);
  builder.SetPrefService(std::move(pref_service));
  browser_state_ = builder.Build();
}

EnterprisePolicyTestHelper::~EnterprisePolicyTestHelper() = default;

TestChromeBrowserState* EnterprisePolicyTestHelper::GetBrowserState() const {
  return browser_state_.get();
}

PrefService* EnterprisePolicyTestHelper::GetLocalState() {
  return local_state_.get();
}

policy::MockConfigurationPolicyProvider*
EnterprisePolicyTestHelper::GetPolicyProvider() {
  return &policy_provider_;
}

BrowserPolicyConnectorIOS*
EnterprisePolicyTestHelper::GetBrowserPolicyConnector() {
  return browser_policy_connector_.get();
}

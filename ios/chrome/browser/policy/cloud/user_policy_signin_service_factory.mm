// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/cloud/user_policy_signin_service_factory.h"

#import "base/memory/ref_counted.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

policy::DeviceManagementService* g_device_management_service_for_testing = NULL;

}  // namespace

namespace policy {

UserPolicySigninServiceFactory::UserPolicySigninServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "UserPolicySigninService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

UserPolicySigninServiceFactory::~UserPolicySigninServiceFactory() {}

// static
UserPolicySigninService* UserPolicySigninServiceFactory::GetForBrowserState(
    web::BrowserState* context) {
  return static_cast<UserPolicySigninService*>(
      GetInstance()->GetServiceForBrowserState(context, true));
}

// static
UserPolicySigninServiceFactory* UserPolicySigninServiceFactory::GetInstance() {
  return base::Singleton<UserPolicySigninServiceFactory>::get();
}

// static
void UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
    DeviceManagementService* device_management_service) {
  g_device_management_service_for_testing = device_management_service;
}

std::unique_ptr<KeyedService>
UserPolicySigninServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  BrowserPolicyConnector* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  // Consistency check to make sure that the BrowserPolicyConnector is available
  // when Enterprise Policy is enabled.
  DCHECK(connector);

  DeviceManagementService* device_management_service =
      g_device_management_service_for_testing
          ? g_device_management_service_for_testing
          : connector->device_management_service();
  DCHECK(device_management_service);

  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);

  return std::make_unique<UserPolicySigninService>(
      chrome_browser_state->GetPrefs(),
      GetApplicationContext()->GetLocalState(), device_management_service,
      chrome_browser_state->GetUserCloudPolicyManager(),
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state),
      browser_state->GetSharedURLLoaderFactory());
}

void UserPolicySigninServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterInt64Pref(policy_prefs::kLastPolicyCheckTime, 0);
}

bool UserPolicySigninServiceFactory::ServiceIsCreatedWithBrowserState() const {
  // When Enterprise Policy is enabled, initialize the UserPolicySigninService
  // early when creating the BrowserState. This will make sure that the user
  // polices are fetched if there is no cache at startup when the account is
  // already syncing and eligible for user policy.
  return true;
}

bool UserPolicySigninServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace policy

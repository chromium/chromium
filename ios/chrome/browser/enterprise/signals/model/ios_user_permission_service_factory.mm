// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/ios_user_permission_service_factory.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/device_signals/core/browser/pref_names.h"
#import "components/device_signals/core/browser/user_permission_service.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/policy/core/common/management/management_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/signals/model/user_delegate_ios.h"
#import "ios/chrome/browser/enterprise/signals/model/user_permission_service_ios.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

IOSUserPermissionServiceFactory::IOSUserPermissionServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "IOSUserPermissionService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(policy::BrowserManagementServiceFactory::GetInstance());
  DependsOn(DeviceTrustConnectorServiceFactoryIOS::GetInstance());
}

void IOSUserPermissionServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  device_signals::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
IOSUserPermissionServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  policy::BrowserManagementService* management_service =
      policy::BrowserManagementServiceFactory::GetForProfile(profile);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  enterprise_connectors::DeviceTrustConnectorService*
      device_trust_connector_service =
          DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile);

  BrowserPolicyConnectorIOS* browser_policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  CHECK(browser_policy_connector);

  auto user_delegate = std::make_unique<UserDelegateIOS>(
      profile->GetPolicyConnector(),
      base::BindRepeating(&BrowserPolicyConnectorIOS::GetDeviceAffiliationIds,
                          base::Unretained(browser_policy_connector)),
      identity_manager, device_trust_connector_service);

  return std::make_unique<UserPermissionServiceIOS>(
      management_service, std::move(user_delegate), profile->GetPrefs());
}

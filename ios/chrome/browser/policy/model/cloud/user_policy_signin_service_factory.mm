// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"

#import "base/memory/ref_counted.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

UserPolicySigninServiceFactory::UserPolicySigninServiceFactory()
    : ProfileKeyedServiceFactoryIOS("UserPolicySigninService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
}

UserPolicySigninServiceFactory::~UserPolicySigninServiceFactory() = default;

// static
UserPolicySigninService* UserPolicySigninServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<UserPolicySigninService>(
      profile, /*create=*/true);
}

// static
UserPolicySigninServiceFactory* UserPolicySigninServiceFactory::GetInstance() {
  return base::Singleton<UserPolicySigninServiceFactory>::get();
}

std::unique_ptr<KeyedService>
UserPolicySigninServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  BrowserPolicyConnector* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  // Consistency check to make sure that the BrowserPolicyConnector is available
  // when Enterprise Policy is enabled.
  DCHECK(connector);

  DeviceManagementService* device_management_service =
      connector->GetTestDeviceManagementService()
          ? connector->GetTestDeviceManagementService()
          : connector->device_management_service();
  DCHECK(device_management_service);

  return std::make_unique<UserPolicySigninService>(
      profile->GetPrefs(), GetApplicationContext()->GetLocalState(),
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile),
      device_management_service, profile->GetUserCloudPolicyManager(),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory());
}

void UserPolicySigninServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterInt64Pref(policy_prefs::kLastPolicyCheckTime, 0);
  user_prefs->RegisterIntegerPref(
      prefs::kProfileSeparationDataMigrationSettings, 1);
}

}  // namespace policy

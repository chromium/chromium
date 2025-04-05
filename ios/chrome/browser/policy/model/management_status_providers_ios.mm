// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/management_status_providers_ios.h"

#import "build/build_config.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/policy/core/common/policy_namespace.h"
#import "components/policy/core/common/policy_service.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace policy {

namespace {

bool IsProfileManaged(ProfileIOS* profile) {
  return profile && profile->GetPolicyConnector() &&
         profile->GetPolicyConnector()->IsManaged();
}

}  // namespace

BrowserCloudManagementStatusProvider::BrowserCloudManagementStatusProvider() =
    default;

BrowserCloudManagementStatusProvider::~BrowserCloudManagementStatusProvider() =
    default;

policy::EnterpriseManagementAuthority
BrowserCloudManagementStatusProvider::FetchAuthority() {
  // A machine level user cloud policy manager is only created if the browser is
  // managed by CBCM.
  if (GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->machine_level_user_cloud_policy_manager() != nullptr) {
    return policy::EnterpriseManagementAuthority::CLOUD_DOMAIN;
  }
  return policy::EnterpriseManagementAuthority::NONE;
}

LocalBrowserManagementStatusProvider::LocalBrowserManagementStatusProvider() =
    default;

LocalBrowserManagementStatusProvider::~LocalBrowserManagementStatusProvider() =
    default;

policy::EnterpriseManagementAuthority
LocalBrowserManagementStatusProvider::FetchAuthority() {
  return GetApplicationContext() &&
                 GetApplicationContext()->GetBrowserPolicyConnector() &&
                 GetApplicationContext()
                     ->GetBrowserPolicyConnector()
                     ->HasMachineLevelPolicies()
             ? policy::EnterpriseManagementAuthority::COMPUTER_LOCAL
             : policy::EnterpriseManagementAuthority::NONE;
}

LocalDomainBrowserManagementStatusProvider::
    LocalDomainBrowserManagementStatusProvider() = default;

LocalDomainBrowserManagementStatusProvider::
    ~LocalDomainBrowserManagementStatusProvider() = default;

policy::EnterpriseManagementAuthority
LocalDomainBrowserManagementStatusProvider::FetchAuthority() {
  auto result = policy::EnterpriseManagementAuthority::NONE;
  if (GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->HasMachineLevelPolicies()) {
    result = policy::EnterpriseManagementAuthority::COMPUTER_LOCAL;
  }
  return result;
}

ProfileCloudManagementStatusProvider::ProfileCloudManagementStatusProvider(
    ProfileIOS* profile)
    : profile_(profile) {}

ProfileCloudManagementStatusProvider::~ProfileCloudManagementStatusProvider() =
    default;

policy::EnterpriseManagementAuthority
ProfileCloudManagementStatusProvider::FetchAuthority() {
  if (IsProfileManaged(profile_)) {
    return policy::EnterpriseManagementAuthority::CLOUD;
  }
  return policy::EnterpriseManagementAuthority::NONE;
}

LocalTestPolicyUserManagementProvider::LocalTestPolicyUserManagementProvider(
    ProfileIOS* profile)
    : profile_(profile) {}

LocalTestPolicyUserManagementProvider::
    ~LocalTestPolicyUserManagementProvider() = default;

policy::EnterpriseManagementAuthority
LocalTestPolicyUserManagementProvider::FetchAuthority() {
  if (!profile_->GetPolicyConnector()->IsUsingLocalTestPolicyProvider()) {
    return policy::EnterpriseManagementAuthority::NONE;
  }
  for (const auto& [_, entry] :
       profile_->GetPolicyConnector()->GetPolicyService()->GetPolicies(
           policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                   std::string()))) {
    if (entry.scope == policy::POLICY_SCOPE_USER &&
        entry.source == policy::POLICY_SOURCE_CLOUD) {
      return policy::EnterpriseManagementAuthority::CLOUD;
    }
  }
  return policy::EnterpriseManagementAuthority::NONE;
}

LocalTestPolicyBrowserManagementProvider::
    LocalTestPolicyBrowserManagementProvider(ProfileIOS* profile)
    : profile_(profile) {}

LocalTestPolicyBrowserManagementProvider::
    ~LocalTestPolicyBrowserManagementProvider() = default;

policy::EnterpriseManagementAuthority
LocalTestPolicyBrowserManagementProvider::FetchAuthority() {
  if (!profile_->GetPolicyConnector()->IsUsingLocalTestPolicyProvider()) {
    return policy::EnterpriseManagementAuthority::NONE;
  }
  for (const auto& [_, entry] :
       profile_->GetPolicyConnector()->GetPolicyService()->GetPolicies(
           policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                   std::string()))) {
    if (entry.scope == policy::POLICY_SCOPE_MACHINE &&
        entry.source == policy::POLICY_SOURCE_CLOUD) {
      return policy::EnterpriseManagementAuthority::CLOUD_DOMAIN;
    }
    if (entry.scope == policy::POLICY_SCOPE_MACHINE &&
        entry.source == policy::POLICY_SOURCE_PLATFORM) {
      return policy::EnterpriseManagementAuthority::DOMAIN_LOCAL;
    }
  }
  return policy::EnterpriseManagementAuthority::NONE;
}

}  // namespace policy

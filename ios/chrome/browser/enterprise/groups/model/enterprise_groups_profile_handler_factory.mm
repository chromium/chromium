// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/groups/model/enterprise_groups_profile_handler_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/enterprise/browser/groups/enterprise_groups_handler.h"
#import "components/enterprise/browser/groups/groups_features.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_groups {

// static
policy::EnterpriseGroupsProfileHandler*
EnterpriseGroupsProfileHandlerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<policy::EnterpriseGroupsProfileHandler>(
          profile, /*create=*/true);
}

// static
EnterpriseGroupsProfileHandlerFactory*
EnterpriseGroupsProfileHandlerFactory::GetInstance() {
  static base::NoDestructor<EnterpriseGroupsProfileHandlerFactory> instance;
  return instance.get();
}

EnterpriseGroupsProfileHandlerFactory::EnterpriseGroupsProfileHandlerFactory()
    : ProfileKeyedServiceFactoryIOS("EnterpriseGroupsProfileHandler",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {}

std::unique_ptr<KeyedService>
EnterpriseGroupsProfileHandlerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(kEnterpriseGroupsExperiments)) {
    return nullptr;
  }
  if (!profile->GetUserCloudPolicyManager()) {
    return nullptr;
  }
  return std::make_unique<policy::EnterpriseGroupsProfileHandler>(
      profile->GetUserCloudPolicyManager()->core(),
      GetApplicationContext()->GetLocalState(), profile->GetProfileName());
}

}  // namespace enterprise_groups

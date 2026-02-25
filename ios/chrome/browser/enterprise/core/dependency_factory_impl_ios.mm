// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/core/dependency_factory_impl_ios.h"

#import "components/policy/core/common/cloud/cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_core {

DependencyFactoryImplIOS::DependencyFactoryImplIOS(ProfileIOS* profile)
    : profile_(profile) {
  CHECK(profile_);
}

DependencyFactoryImplIOS::~DependencyFactoryImplIOS() = default;

policy::CloudPolicyManager*
DependencyFactoryImplIOS::GetUserCloudPolicyManager() const {
  return profile_->GetUserCloudPolicyManager();
}

}  // namespace enterprise_core

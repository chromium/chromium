// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CORE_DEPENDENCY_FACTORY_IMPL_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CORE_DEPENDENCY_FACTORY_IMPL_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/enterprise/core/dependency_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_core {

class DependencyFactoryImplIOS : public DependencyFactory {
 public:
  explicit DependencyFactoryImplIOS(ProfileIOS* profile);

  ~DependencyFactoryImplIOS() override;

  // DependencyFactory:
  policy::CloudPolicyManager* GetUserCloudPolicyManager() const override;

 private:
  const raw_ptr<ProfileIOS> profile_;
};

}  // namespace enterprise_core

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CORE_DEPENDENCY_FACTORY_IMPL_IOS_H_

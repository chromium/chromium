// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_GROUPS_MODEL_ENTERPRISE_GROUPS_PROFILE_HANDLER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_GROUPS_MODEL_ENTERPRISE_GROUPS_PROFILE_HANDLER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace policy {
class EnterpriseGroupsProfileHandler;
}  // namespace policy

class ProfileIOS;

namespace enterprise_groups {

class EnterpriseGroupsProfileHandlerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static policy::EnterpriseGroupsProfileHandler* GetForProfile(
      ProfileIOS* profile);
  static EnterpriseGroupsProfileHandlerFactory* GetInstance();

 private:
  friend class base::NoDestructor<EnterpriseGroupsProfileHandlerFactory>;

  EnterpriseGroupsProfileHandlerFactory();
  ~EnterpriseGroupsProfileHandlerFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace enterprise_groups

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_GROUPS_MODEL_ENTERPRISE_GROUPS_PROFILE_HANDLER_FACTORY_H_

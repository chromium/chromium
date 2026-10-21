// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_GROUPS_MODEL_ENTERPRISE_GROUPS_PROFILE_HANDLER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_GROUPS_MODEL_ENTERPRISE_GROUPS_PROFILE_HANDLER_FACTORY_H_

#import "components/enterprise/browser/groups/enterprise_groups_handler.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace enterprise_groups {

class EnterpriseGroupsProfileHandlerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          EnterpriseGroupsProfileHandlerFactory,
          policy::EnterpriseGroupsProfileHandler> {
 public:
  explicit EnterpriseGroupsProfileHandlerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace enterprise_groups

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_GROUPS_MODEL_ENTERPRISE_GROUPS_PROFILE_HANDLER_FACTORY_H_

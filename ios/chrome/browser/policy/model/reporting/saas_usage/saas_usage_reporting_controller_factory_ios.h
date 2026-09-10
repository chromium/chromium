// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_FACTORY_IOS_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace enterprise_reporting {

class SaasUsageReportingController;

class SaasUsageReportingControllerFactoryIOS
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static SaasUsageReportingController* GetForProfile(ProfileIOS* profile);
  static SaasUsageReportingControllerFactoryIOS* GetInstance();

 private:
  friend class base::NoDestructor<SaasUsageReportingControllerFactoryIOS>;

  SaasUsageReportingControllerFactoryIOS();
  ~SaasUsageReportingControllerFactoryIOS() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_FACTORY_IOS_H_

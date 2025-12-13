// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_IOS_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace enterprise_reporting {

class CloudProfileReportingServiceIOS;

class CloudProfileReportingServiceFactoryIOS
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static CloudProfileReportingServiceFactoryIOS* GetInstance();
  static CloudProfileReportingServiceIOS* GetForProfile(ProfileIOS* profile);

  CloudProfileReportingServiceFactoryIOS(
      const CloudProfileReportingServiceFactoryIOS&) = delete;
  CloudProfileReportingServiceFactoryIOS& operator=(
      const CloudProfileReportingServiceFactoryIOS&) = delete;

 protected:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;

 private:
  friend base::NoDestructor<CloudProfileReportingServiceFactoryIOS>;

  CloudProfileReportingServiceFactoryIOS();
  ~CloudProfileReportingServiceFactoryIOS() override;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_IOS_H_

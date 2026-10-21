// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_IOS_H_

#import "ios/chrome/browser/policy/model/reporting/cloud_profile_reporting_service_ios.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace enterprise_reporting {

class CloudProfileReportingServiceFactoryIOS final
    : public TypedProfileKeyedServiceFactoryIOS<
          CloudProfileReportingServiceFactoryIOS,
          CloudProfileReportingServiceIOS> {
 public:
  explicit CloudProfileReportingServiceFactoryIOS(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_IOS_H_

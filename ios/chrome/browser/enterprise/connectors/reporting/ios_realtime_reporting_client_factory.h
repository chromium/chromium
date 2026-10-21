// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_FACTORY_H_

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace enterprise_connectors {

class IOSRealtimeReportingClientFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSRealtimeReportingClientFactory,
          IOSRealtimeReportingClient> {
 public:
  explicit IOSRealtimeReportingClientFactory(PassKey key);

  // Returns the default factory used to build IOSRealtimeReportingClient. Can
  // be registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_FACTORY_H_

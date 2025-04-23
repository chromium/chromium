// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace enterprise_connectors {

class IOSRealtimeReportingClient;

class IOSRealtimeReportingClientFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  IOSRealtimeReportingClientFactory(const IOSRealtimeReportingClientFactory&) =
      delete;
  IOSRealtimeReportingClientFactory& operator=(
      const IOSRealtimeReportingClientFactory&) = delete;

  // Returns the instance of IOSRealtimeReportingClientFactory.
  static IOSRealtimeReportingClientFactory* GetInstance();

  // Returns the IOSRealtimeReportingClient for `profile`, creating it if it is
  // not yet created.
  static IOSRealtimeReportingClient* GetForProfile(ProfileIOS* profile);

  // Returns the default factory used to build IOSRealtimeReportingClient. Can
  // be registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 protected:
  // BrowserStateKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;

 private:
  friend class base::NoDestructor<IOSRealtimeReportingClientFactory>;

  IOSRealtimeReportingClientFactory();
  ~IOSRealtimeReportingClientFactory() override;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REALTIME_REPORTING_CLIENT_FACTORY_H_

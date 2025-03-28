// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REPORTING_EVENT_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REPORTING_EVENT_ROUTER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace enterprise_connectors {

class IOSReportingEventRouterFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the instance of IOSReportingEventRouterFactory.
  static IOSReportingEventRouterFactory* GetInstance();

  // Returns the IOSReportingEventRouter for `profile`, creating it if it is not
  // yet created.
  static ReportingEventRouter* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<IOSReportingEventRouterFactory>;

  IOSReportingEventRouterFactory();
  ~IOSReportingEventRouterFactory() override;

  // BrowserStateKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REPORTING_EVENT_ROUTER_FACTORY_H_

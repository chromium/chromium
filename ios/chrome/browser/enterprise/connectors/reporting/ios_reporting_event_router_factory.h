// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REPORTING_EVENT_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REPORTING_EVENT_ROUTER_FACTORY_H_

#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace enterprise_connectors {

class IOSReportingEventRouterFactory final
    : public TypedProfileKeyedServiceFactoryIOS<IOSReportingEventRouterFactory,
                                                ReportingEventRouter> {
 public:
  explicit IOSReportingEventRouterFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_IOS_REPORTING_EVENT_ROUTER_FACTORY_H_
